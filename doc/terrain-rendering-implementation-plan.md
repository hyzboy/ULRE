# GPU-Driven 地形渲染实现方案（MeshShader / BDA / LOD）

> **来源**：把 OpenGL 4.6 地形工程（`D:\AIProgramming\Terrain`，下文称"源工程"）的地形技术
> 迁入 ULRE 的 Vulkan + MeshShader 管线。**不是逐行翻译**——源工程的数据组织方式（VBO/EBO、
> CPU 分块、法线图烘焙）在 ULRE 里大部分要换掉，只保留技术原理。
>
> **状态**：设计定稿，未执行。T1–T5 每阶段以「改 → `cmake --build` → 运行验证 → commit」收尾，
> 每阶段必须先通过验证矩阵中对应判据才进入下一阶段。
>
> **关联文档**：`doc/meshlet-geometry-pipeline-implementation.md`（MeshDrawParams 112B 契约、
> 间接绘制双轨分流）、`doc/gpu-driven-4id-draw-item-plan-2026-09.md`（4-ID 与全局池基线）、
> `doc/render-item-descriptor-and-indirection-pipeline.md`（两级间接）、
> `doc/ecs-layer-architecture-and-frame-flow.md`（相位与帧流）、`doc/backlog.md`（A1 在途帧资源现状）。

---

## 0. 决策速览

| 议题 | 决策 | 依据 |
|---|---|---|
| 网格生成 | MeshShader 内由索引生成 XY，Z 取高度场 | 无 VBO/EBO，CPU 只提交 tile 记录 |
| 顶点数据源 | 无顶点缓冲（`VertexInputMode::None` 同款自持路径） | CharQuad 模式已有先例 |
| 高度存储 | **BDA storage buffer**（非 texture、非 sparse） | mesh 阶段无法访问 bindless 纹理集；引擎全 BDA |
| 分块 | 两级：**页**（流式单位）+ **tile**（绘制单位） | 避免 tile 跨页两级间接 |
| Texture2DArray | 不用 | 层数上限 256；整数组共享一条 mip 链，无法按层流式 |
| Sparse texture | **不做**（远期可选） | 引擎零 sparse 支持；tile 表已承担虚拟化 |
| Mipmap | 高度图**不做 mip**；做 per-tile min/max 金字塔 | mip 破坏 LOD 无缝前提；远景开销在顶点数不在采样 |
| 参数传递 | 整数域贯穿到**算出缓冲偏移**为止 | 浮点仅用于最终坐标/高度 |
| 世界坐标精度 | CPU double + tile 局部小浮点 | camera-relative 尚未启用（`Camera.cpp:64-65`） |
| 绘制路径 | 专用 `TerrainRenderPipeline`（照 `LineRenderPipeline` 四件套） | 不改通用 4-ID 批处理 |
| 裂缝处理（LOD 接缝） | 2 的幂步长保证**顶点级重合**；边内部 T 型缝用**裙边**消除（形变为可选） | 见 §3.3 |
| 远景降载 | 2 的幂 LOD（步长翻倍 → 顶点数 1/4） | 顶点/图元数才是瓶颈 |

---

## 1. 源工程分析与评价

### 1.1 技术原理链（七环）

| 环 | 实现 | 位置 |
|---|---|---|
| 1 网格 | **无 VBO**，只有 EBO（`(N-1)²×6` 索引）；顶点位置由 `gl_VertexID` 在 VS 内生成 | `terrain_grid.cpp:5-30`、`shaders/terrain.vert:11-33` |
| 2 高度场 | 径向正弦环（`sin(r*freq*2π+phase)`），`R16UI` 16 位整数高度 | `terrain_procedural.cpp:8-32` |
| 3 分块 | 大高度场按 `gridDim=8 / blockSize=128` 切成 64 块，带 1 行/列重叠 | `terrain_procedural.cpp:89-110`、`terrain_manager.h:13-15` |
| 4 法线 | CPU 中心差分烘成 `RGB8` 法线图，逐块生成 | `terrain_procedural.cpp:34-66` |
| 5 装载 | 两张 `texture2DArray`（高度 64 层 `R16UI` + 法线 64 层 `RGB8`） | `terrain_manager.cpp:15-23` |
| 6 绘制 | 64 次 `glDrawElements`，每块一个平移模型矩阵 + `uTexLayer` | `terrain_manager.cpp:26-44` |
| 7 着色 | 顶点用 `usampler2DArray` + `texelFetch` 取高度位移；片元贴法线图做单光照明 | `shaders/terrain.vert:21-33`、`shaders/terrain.frag:9-16` |

坐标系为 **Z-up**（`camera.h:8-9` 注视方向与 `terrain.vert:26-29` 的 XY 平面 + Z 高度一致）。

### 1.2 已确认缺陷（迁入前必须知道，避免"把 bug 一起搬过去"）

1. **法线图块边界被钳制 → 块间可见亮度缝。** `getH()` 在块外把坐标 clamp 到 `[0,size-1]`
   （`terrain_procedural.cpp:38-42`），所以每块首/末行列的偏导只有正常值的一半；而重叠行列
   的数据其实已经算好了却没被使用。→ ULRE 方案改用**屏幕空间导数求法线**，从根上消除。
2. **法线图 UV 半纹素与越界采样。** `vUV=(gx,gy)/(uSize-1)` 配上 `GL_LINEAR + GL_REPEAT`
   （`terrain.vert:16-19` + `texture2d.cpp:73-76`），坐标正好落在 0/1 上会跨到对边纹理。
   正确写法是 `(gx+0.5)/uSize`。→ 缓冲方案下不存在该问题（整数 texel 索引）。
3. **零剔除。** 64 块每帧无条件全画（`terrain_manager.cpp:33-41`），
   约 203 万三角/帧（64×31752），无 LOD、无视锥裁剪。
4. **无法独立缩放高度。** `uScale` 是 `vec3` 但着色器里 `pos *= uScale` 对 XYZ 同乘
   （`terrain.vert:29-30`），模型矩阵只做平移（`terrain_manager.cpp:36`），
   于是 16 单位起伏铺在约 1000 单位横向跨度上（极平）。
5. **工程性问题**：着色器按相对路径加载（`main.cpp:67`，依赖工作目录）；
   `CMakeLists.txt:35` 引用了不存在的变量 `${Stb_INCLUDE_DIR}`。

### 1.3 保留 / 替换对照

| 源工程技术 | ULRE 方案 | 说明 |
|---|---|---|
| `gl_VertexID` 生成 XY | **保留**，改为 MeshShader 索引生成 | 核心原理不变 |
| 高度取自 16 位整数高度场 | **保留**，改 BDA `uint16` 缓冲 + **双线性** | 换来 LOD 无缝 |
| 1 行/列重叠分块 | **替换**：tile 表 + 页偏移 | 重叠是为了缝数据；改为按需越界采样 |
| CPU 生成法线图 | **替换**：`ntb_derivative_normalmap.glsl` | 消除缺陷 1、2 |
| 整块一次 draw | **替换**：1 tile = N 个 workgroup；后续间接 multi-draw | Mesh 输出容量限制 |
| 64 层 Texture2DArray | **替换**：页/tile 整数偏移 | 层数上限与流式 |
| 平移矩阵 + `uScale` | **替换**：tile 世界原点（整数）+ 高度缩放标量 | 修复缺陷 4 |

---

## 2. ULRE 侧现状基线（唯一真源，逐条附证据）

改任何东西之前先读本节；下面每条都已在源码中核对过。

### 2.1 顶点阶段只有 MeshShader，且模式由注册表正向声明

- 模式枚举：`VertexPassthrough` / `LineQuad` / `CharQuad`（`inc/hgl/mtl/MeshShaderMode.h:9-14`），
  解析在 `:18-29`，每线程输出量在 `:39-50`。
- 模式描述符（9 个函数指针：拓扑/Defines/UBO/专属资源/stage1/2/3/主体）
  `src/ShaderGen/meshgen/MeshModeDescriptor.h:67-79`，注册表 `:222-269`。
- **`VertexPassthrough` 的图元循环是"连续三点一组"**（`ShaderLibrary/mesh/vertex_passthrough.glsl.tmpl:23-27`），
  只能表达三角汤，**不能表达共享顶点的网格** → 地形必须新增模式。
- **`CharQuad` 拓扑 = 每线程 1 个 quad（4 顶点 + 2 图元）**
  （`MeshModeDescriptor.h:100-107` + `ShaderLibrary/mesh/char_quad.glsl.tmpl`：
  `SetMeshOutputsEXT(count*4, count*2)`、`base_vid = gl_LocalInvocationIndex*4`、索引 `(0,1,2)/(2,1,3)`）。
  **地形的一个格与一个字符 quad 拓扑完全同构** → 新模式的实现成本主要是一份模板，不是新机制。
- CharQuad 是"自持全部数据、无外部顶点输入"的先例：
  描述符里 stage1/2/3 填 `nullptr`（`MeshModeDescriptor.h:250-260`），
  自带 SSBO 声明（`MeshShaderModeCharQuad.h:18-23` → `ShaderLibrary/vertex/s1_text_char_quad.glsl`）。

### 2.2 顶点输入统一为 SSBO/BDA；`None` 输入已有先例

- `s1_position_vec3.glsl:18`：`Position = sbo_vertex_position.data[draw_params.vertex_base + VertexIndexID]`。
- `VertexInputMode::None = 无外部顶点输入`（`inc/hgl/mtl/VertexShaderNodeConfig.h:12`）。
- `VertexABIBuilder.cpp:266` 是"无外部顶点输入"的分支点。

### 2.3 三张 BDA 契约表（改字段只改单一真源）

| 结构 | 大小 | 位置 | 用途 |
|---|---|---|---|
| `MeshDrawParams` | **112B**（6×4B 头 + 11×uint64 地址尾） | `inc/hgl/graph/ShaderBufferSources.h:16-33`，断言 `:85-88` | 每 draw 的几何/地址行 |
| `MeshDrawCommand` | 8B（`geometry_id` + `first_instance`） | `:93-127` | 间接命令面 |
| `DrawItem4ID` | 16B（transform/geometry/material/texture） | `:148-186` | 4-ID 渲染项 |
| `GeometryAABB` | 32B（center.xyz+r / extents.xyz） | `:188-198` | 计算着色器视锥剔除用 |
| **`RootAddresses`** | **72B**（8×uint64 + 2×uint32） | `:206-216`，断言 `:245-264` | push constant 承载的全局表地址 |

全部由 `HGL_*_FIELD_LIST` 宏列表单源生成（CPU 成员 / GLSL 字段名 / GLSL 类型 / 布局断言），
GLSL 侧由 `MeshShaderHeaderGen.h:29+`（mesh 阶段）与 `MaterialShaderEmitter.cpp:512+`（片元阶段）遍历发射。

### 2.4 关键先例：`RootAddresses` 承载全局数据表

`ShaderLibrary/vertex/s1_text_char_quad.glsl:45` 的写法就是模板：

```glsl
layout(buffer_reference, scalar, buffer_reference_align=16) buffer TextCharInfoRef { TextCharInfo chars[]; };
#define sbo_char_info TextCharInfoRef(pc_root.addr_text_char_info)
```

文本三表（`addr_text_char_info/style/instance`）正是这样挂进 `RootAddresses` 的
（`ShaderBufferSources.h:212-214`）→ **地形表照此办理即可，不需要任何描述符/布局改动**。

### 2.5 专用渲染管线的四件套先例

`inc/hgl/ecs/support/line/{LineCollectSystem,LineBuildSystem,LineRenderPipeline,LineRenderSystem}.h`
+ `src/ecs/support/line/*.cpp`，注册点 `src/ecs/core/DefaultSystems.cpp:136`
（`std::make_unique<LineRenderPipeline>(ctx)`），通过 `world->GetRenderPipeline(LineRenderPipeline::kName)` 取用
（`src/ecs/systems/render/LineStatsSystem.cpp:26`）。

`LineRenderPipeline::Draw()` 就是"自持 SSBO + 一次 `cmd->DrawMeshTasks(group_count)`"
（`src/ecs/support/line/LineRenderPipeline.cpp:260-286`），draw 前自行 push 根地址（`:762`）。
**地形管线照抄这个壳。**

### 2.6 间接绘制与 GPU count 已 API 化（T4 用）

- `Device::CreateDrawCountBuffer(...)`：`INDIRECT|STORAGE|TRANSFER_DST|SHADER_DEVICE_ADDRESS`
  （`inc/hgl/vk/VKDevice.h:384-391`）。
- `MaterialBatch::icb_count_buffer` 挂上即走 Indirect Count Multi-Draw；
  端到端示例 `example/Basic/ComputeIndirectCount.cpp`（`GPUIndirectCountHookSystem` 在
  `RenderFrameSync` 阶段挂 count buffer）。
- 多命令一次提交：`PipelineMaterialRenderer.cpp:52-65`（`DrawMeshTasksIndirectCount`，
  `gl_DrawID` 索引 `MeshDrawParams` 行，命令序 = 行序）。
- 双轨分流现状：含 meshlet 的几何按 `meshlet_count` 发射，其余走 `VertexPassthrough`
  （`doc/meshlet-geometry-pipeline-implementation.md` §6.3，`PrimitiveBatchPipeline.cpp` 写命令处）。

### 2.7 限制一：mesh 阶段**不能**采样 bindless 纹理

`src/Vulkan/VKBindlessTextureManager.cpp:42/48/54`：bindless 纹理/Cube/采样器三个 binding 的
`stageFlags` 都是 `FRAGMENT|COMPUTE`。mesh 阶段访问会违反描述符-阶段一致性
（"shader uses descriptor slot but descriptor not accessible from stage"）。
→ 地形高度走 BDA 缓冲，与引擎既有约定一致，**零描述符改动**。

（若将来一定要 mesh 阶段采样纹理：需给这三个 binding 加 `VK_SHADER_STAGE_MESH_BIT_EXT`
/`TASK`，并确认所有材质管线布局兼容——属于独立变更，不在本方案内。）

### 2.8 限制二：camera-relative 尚未启用

- `CameraInfoData` 已有 `pos`（视口相对）与 `camera_world_pos`（绝对世界，低精度）
  （`ShaderLibrary/ubo/scene_ubo.glsl`；CPU 侧 `src/SceneGraph/camera/Camera.cpp:56-62`，
  注释明写"供 fog/terrain 等 shader 使用"）。
- **但**`Camera.cpp:64-65` 明确：视图矩阵平移归零与 `pos=0` **暂不启用**，
  需等 `TransformAssignmentBuffer::SetCameraOffset` 完整接入。
- 结论：地形**不能**依赖 camera-relative。精度策略 = CPU 侧 `world_position_double` 累加 +
  shader 内 tile 局部小坐标（≤2²⁴ 的整数在 float 中精确）。大世界精度接入列为 T5。

### 2.9 限制三：在途帧资源仍是单份

`doc/backlog.md` A1：Camera/Viewport UBO 与 L2W ring 段是**单份 host-visible 内存**，
per-frame 多份化未做。→ 地形的"compute 写 tile 表 / 间接命令"必须**自己按帧多份化**
（环形或双缓冲），不能指望引擎兜住写-读竞态。

### 2.10 占位现状：`PositionSourceSpec::TerrainHeightmapGrid` 是个空壳

枚举已存在（`inc/hgl/ecs/support/PositionSourceSpec.h:8-14`），
但只在 `PrimitiveBatchPipeline.cpp:287-312` 的 `switch` 里与其它情况合并透传，**无任何实现**。
→ 本方案不走通用批处理，因此该分支保持现状；但它说明"地形高度网格"在引擎设计里早被预留。

### 2.11 可用的缓冲创建 API

- `CreateSSBO(...)` → `STORAGE | SHADER_DEVICE_ADDRESS`（`inc/hgl/vk/VKDevice.h:315`，
  `CREATE_BUFFER_OBJECT` 宏展开），BDA 地址取用 `GetBufferDeviceAddressAligned16`（`:365`）。
- 全局池：`GlobalSSBOBufferRegistry::GetAccessor<T>()` / `Acquire(GlobalSSBOType)`
  （`inc/hgl/graph/module/GlobalSSBOBufferRegistry.h:135-205`），
  类型枚举在 `inc/hgl/graph/ssbo/SSBOTypes.h:14-23`，行类型 traits 在
  `inc/hgl/graph/ssbo/MaterialSSBOLayout.h:36-50`。

---

## 3. 目标架构

### 3.1 组件与数据流

```
[高度数据]  高度页缓冲（BDA uint16，页对齐）
[页表]      每页：texel 尺寸 / 缓冲段偏移 / 驻留标志
[tile 表]   每 tile：texel 原点(整数) / grid_dim / lod / 高度界 / flags   ← 唯一"画在哪"的权威
        │
        ▼  （T1-T3：CPU 合成；T4：compute 生成）
[间接命令]  VkDrawMeshTasksIndirectCommandEXT[] + count        （T4）
        │
        ▼
TerrainRenderPipeline（专用管线，四件套）
        │  1 次 DrawMeshTasks（T1-T3）/ DrawMeshTasksIndirectCount（T4）
        ▼
MeshShader「TerrainGrid」模式：1 线程 = 1 格 = 4 顶点 + 2 图元
   uv/texel = tile.texel_origin + (local << lod)     ← 全整数
   h        = bilinear(高度页, texel)                 ← 全整数寻址，最后转 float
   world_xy = texel * 全局 texel 世界尺寸             ← 最后一步
   位置     = camera.vp * vec4(world_xy, h * height_scale, 1)
```

**核心不变式**：一个 tile 只有一条"读什么数据、画在哪"的信息（texel 原点 + lod + 页偏移），
采样位置与世界坐标由它派生。不存在第二处权威（源工程的"L2W 平移 + uTexLayer + uSize"
三处分散信息被收敛为一条记录）。

### 3.2 结构定义（唯一真源，C++ 侧）

```cpp
// inc/hgl/graph/ssbo/TerrainRows.h（新增）
namespace hgl::graph::ssbo
{
    // 页：流式单位。页尺寸固定（如 256×256 texel），texel 存储在其缓冲段内。
    struct TerrainPage                       // 32B
    {
        uint32_t buffer_offset;              // 页数据在高度缓冲中的首 texel 偏移（整数）
        uint32_t resident;                   // 0/1，驻留标志（T5 流式用）
        uint32_t page_texels;                // 页边长（texel），固定值冗余存储便于自检
        uint32_t _pad;
        float    height_min;                 // 页内高度界（剔除用）
        float    height_max;
        uint32_t _pad2[2];
    };
    static_assert(sizeof(TerrainPage) % 16 == 0);

    // tile：绘制单位。"画在哪"的唯一权威。
    struct TerrainTile                       // 32B
    {
        uint32_t texel_origin_x;             // 全局 texel 原点（整数，精确）
        uint32_t texel_origin_y;
        uint32_t grid_dim;                   // 顶点边数（cells = grid_dim-1）
        uint32_t lod;                        // 网格步长 = 1 << lod（texel）；0 = 最细
        float    height_min;                 // 本 tile 高度界（LOD 误差度量 + 粗剔除）
        float    height_max;
        uint32_t page_index;                 // 指向页表（T5 流式用；T1-T4 填 0）
        uint32_t flags;
    };
    static_assert(sizeof(TerrainTile) % 16 == 0);
}
```

两个结构都按引擎惯例补到 16B 整倍数（`MaterialDataRows.h:40-45` 同款断言风格），
并同时提供 `HGL_*_FIELD_LIST` 供 GLSL 侧声明与布局断言复用（§2.3 的单一真源机制）。

### 3.3 LOD 接缝（裂缝）的本质与消除手段

> **更正**：本方案早期版本曾断言"2 的幂步长 + 双线性采样即可无缝、不需要裙边"——**该结论错误**，
> 现按下方结论执行。差别很重要：它决定了 T3 是否必须实现裙边。

**T 型交点缝是固有几何问题，不能靠采样方式消除。**

- 相邻 LOD 共享一条边时：细侧是**折线**（穿过多个顶点），粗侧是**弦**（只连两端点）。
  两者只有在该边上的高度函数为线性（平坦或恒定坡）时才重合。
- "2 的幂步长 + 同一高度函数"能保证的是**顶点级重合**：粗格点位置是细格点子集 →
  取到同一个 texel → 同一个高度值。这**不等于**边内部重合。

三种工程手段：

| 手段 | 说明 | 代价 | 本方案 |
|---|---|---|---|
| **a. 裙边（skirt）** | tile 边缘额外向下挤出一圈三角形遮住缝隙 | 少量额外三角形；需确定深度 | **T3 默认** |
| b. 顶点形变（geomorph） | 细侧过渡带顶点向粗侧弦插值 | 需 morph 因子与过渡带判定 | T3+ 可选优化 |
| c. 过渡带缝合 | 粗 tile 外圈改用细步长 | tile 需知邻居 LOD，网格不再均匀 | 不采用 |

**双线性采样的真实作用**：把"高度函数"与"网格分辨率"解耦（网格点落在 texel 之间时避免阶梯），
并让不同 LOD 在任意位置取到同一函数值——这使裂缝**有界且可量化**（是 a/b 的前提）。
在"2 的幂 + texel 对齐"时它退化为直接取纹素（fraction = 0），因此**它不是消裂手段**。

**裙深怎么定（可量化）**：裂缝深度 = 共享边上 `|粗表面高度 − 细表面高度|` 的最大值。
两种取法：(1) 保守 `skirt_depth = k·(height_max − height_min)`（k ≈ 0.5~1，用 §3.2 已有的
高度界字段，零额外数据）；(2) 精确：由父级 tile 的误差度量给出（更紧，但需层级数据）。
判据：`skirt_depth ≥ 实测最大裂缝深度`（见 T3 与 §5）。

**对角线约定**：格内固定按 `(TL,BL,TR)+(TR,BL,BR)` 或 `(TL,TR,BR)+(TL,BR,BL)` 之一对角化，
全局一致（不一致只造成亚 texel 级差异，但保持一致成本为零）。

**LOD 级差约束**：网格步长必须为 2 的幂（粗格点坐标 ⊂ 细格点坐标）——主流地形方案的通行约束，
本方案沿用。它保证顶点级重合，把裂缝压到"可量化的最小"。

### 3.4 数据注入通路（推荐 A，备选 B）

**A（推荐）：扩展 `RootAddresses`。** 与文本三表完全同构：
`HGL_ROOT_ADDRESSES_FIELD_LIST` 追加 `addr_terrain_pages` / `addr_terrain_tiles`，
push constant 72B → 88B（规范保证下限 128B，安全）。
- 收益：单一真源、零描述符改动、mesh 阶段可直接引用（`pc_root.addr_terrain_tiles`）。
- 成本：全局 push constant 结构变化（两侧均由同一宏列表生成，无手工同步风险）。
- 必须同时更新 `RootAddressesLayoutValid()`（`ShaderBufferSources.h:245-264`）里
  硬编码的 `i < 8` 与 `offsets[8]/[9]` 常量。

**B（备选）：新增全局行类型**（`GlobalSSBOType` + `GlobalRowTypeTraits` + 材质行里存 uint64 地址）。
零全局 push constant 改动，但要新增行类型、材质 recipe 接线，且地址要经 `addr_mtl_data_addrs`
二级解引用，链路更长。仅当"不允许动全局 push constant"时选它。

### 3.5 参数传递的整数纪律

```
cell_id   = gl_WorkGroupID.x * group_size + gl_LocalInvocationIndex   // uint
gx        = (cell_id % cells_per_row)                                  // 整数
gy        = (cell_id / cells_per_row)
texel_x   = tile.texel_origin_x + (gx << lod)                          // 整数
index     = texel_y * stride + texel_x                                 // 整数 ← 到这里为止全整数
h         = float(heights[index])                                      // 最后才转 float
world_xy  = float(texel) * terrain_texel_size                          // 最后一步
```

- 小整数（≤2²⁴）的 float 是精确的，所以网格局部坐标与 texel 索引不怕 float；
- **怕的是世界坐标与"uv→世界"的来回乘除** → 世界坐标只在 CPU 侧以 double 累加，
  shader 内只出现 tile 局部小坐标。
- tile 记录全部字段为整数（除两个 float 高度界）→ CPU/compute 两侧同布局，`static_assert` 卡死漂移。

---

## 4. 分阶段实施

### T1 — 单 tile 出图（MeshShader 新模式 + BDA 高度读取）

**目标**：跑通"mesh 阶段生成 XY → 从 BDA 缓冲读高度 → 位移出图"这条链路，不涉及分块/LOD。

**新增文件**

| 文件 | 内容 |
|---|---|
| `ShaderLibrary/mesh/terrain_grid.glsl.tmpl` | 主体模板（照 `char_quad.glsl.tmpl` 结构，**不**含 `{{}}` 占位之外的分支） |
| `ShaderLibrary/vertex/s1_terrain_grid.glsl` | `TerrainHeightRef` / `TerrainTileRef` buffer_reference 声明 + `#define sbo_terrain_*` 垫片（照 `s1_text_char_quad.glsl`） |
| `src/ShaderGen/meshgen/MeshShaderModeTerrainGrid.h` | `EmitTerrainGridSSBODeclarations` + `EmitTerrainGridBody`（照 `MeshShaderModeCharQuad.h`） |
| `example/Terrain/TerrainBasic.cpp` + `example/Terrain/CMakeLists.txt` | 验证示例 |

**改动触点（8 处，缺一不可）**

| # | 文件 | 改动 |
|---|---|---|
| 1 | `inc/hgl/mtl/MeshShaderMode.h:9-14` | 加 `TerrainGrid` 枚举值 |
| 2 | 同上 `:18-29` | `ParseMeshShaderMode` 加分支（**拼错必须显式失败**，不允许静默降级） |
| 3 | 同上 `:39-50` | `GetMeshModeVerticesPerInvocation`=4 / `PrimitivesPerInvocation`=2 |
| 4 | `src/ShaderGen/meshgen/MeshModeDescriptor.h` | 新增 `ResolveTerrainGridTopology`（与 CharQuad 同算式，**独立命名**以便将来单线程 2 格等变体）+ 注册表第 4 项（defines 用 `EmitStandardMeshDefines`；`ubos` = ViewportInfo + CameraInfo，**不含 l2w**；custom resources = 地形 SSBO 声明；stage1/2/3 = `nullptr`） |
| 5 | `src/ShaderGen/builder/GenericMaterialBuilder.cpp:429-456` | 模式决策链加 `TerrainGrid` 分支（含 `max_invocations` 与容量钳制，见下） |
| 6 | `src/ShaderGen/material_definition/MaterialDefinitionFile.cpp:1080-1091`、`:1189-1191` | `[mesh_shader] mode` 已知键校验允许 `TerrainGrid` |
| 7 | `src/ShaderGen/builder/VertexABIBuilder.cpp:266` 附近 | 与 CharQuad 同类的"无外部顶点输入"分支 |
| 8 | `inc/hgl/graph/ShaderBufferSources.h:206-216`、`:245-264` | `RootAddresses` 加 `addr_terrain_pages` / `addr_terrain_tiles` + 断言更新；`inc/hgl/graph/RootAddressPush.h:28-39` 加参（**先做 §6 风险 R1 的结构体化**） |

**容量与分组（Mesh 输出限制）**

- 每线程 = 1 格 = 4 顶点 + 2 图元；`local_size_x = 64` → 每组覆盖 8×8 格。
- `max_vertices = 256 / max_primitives = 128`，与 CharQuad 同卡规范保证下限
  （`GenericMaterialBuilder.cpp:438` 注释即此意）。
- tile 的组数 = `ceil(cells/8)²`（`cells = grid_dim-1`）；**边缘组存在无效格**。

**⚠ 头号实现陷阱（T1 就必须正确处理）**

`SetMeshOutputsEXT` 要求**同组所有 invocation 传入相同的值**，而边缘组的有效格数不是组内线程的
简单 `min`。必须：

```glsl
// 全组一致地计算本组有效格数（2D 裁剪）
const uint cols = min(8u, cells - gx0);     // gx0 = 组起始格列
const uint rows = min(8u, cells - gy0);
const uint valid = (gx0 >= cells || gy0 >= cells) ? 0u : cols * rows;
SetMeshOutputsEXT(valid * 4u, valid * 2u);
if (lx >= cols || ly >= rows) return;         // 本线程无有效格
const uint compact = ly * cols + lx;          // ← 紧凑槽位，不要用 gl_LocalInvocationIndex
const uint base_vid = compact * 4u;
```

用 `gl_LocalInvocationIndex` 直接算 `base_vid` 会在边缘组产生空洞（写到超出 `valid*4` 的槽位）。

**地形材质（`ShaderLibrary/material/terrain.material.toml`，照 `text_2d_gpu.material.toml`）**

```toml
schema = 3
id = "Terrain"
name = "Terrain"
provider_policy = "GeometryOnly"

[mesh_shader]
mode = "TerrainGrid"
max_invocations = 64

[transform]
source = "None"
mapping = "Passthrough3D"
orientation = "World"
scale = "World"
projection = "WorldCameraVP"

[fragment]
material_source_module = "material/terrain_source.glsl"      # 新增：地势配色（可先取纯色）
ntb_module = "ntb/ntb_derivative_normalmap.glsl"             # 用现有导数法线，不烘法线图

[vertex]
requirements = ["Position"]
varyings = ["emit_world_pos", "emit_world_normal", "emit_uv0"]

[resources]
ubos = ["CameraInfo"]
samplers = ["Trilinear", "Linear", "ShadowPCF"]
```

**T1 验证**：`example/Terrain/TerrainBasic.cpp`（`RunFramework<TestApp>` 模式，
`example/CMakeLists.txt` 加 `add_subdirectory(Terrain)`，
CMakeLists 内 `CreateProject(TerrainBasic TerrainBasic.cpp)`，
`FOLDER "ULRE/Example/Terrain"`，工作目录 `ULRE_RUNTIME_PATH`）。
高度数据 T0 阶段由 CPU 生成正弦环写入 `CreateSSBO` 缓冲（单页、1025² 可直接放 128 格网格）。

### T2 — 多 tile（CPU 合成 tile 表 + 专用管线）

**目标**：整个地形由多 tile 组成，CPU 侧合成 tile 表；引入剔除的基础设施。

- 新增四件套：`inc/hgl/ecs/support/terrain/{TerrainCollectSystem,TerrainBuildSystem,TerrainRenderPipeline,TerrainRenderSystem}.h`
  + `src/ecs/support/terrain/*.cpp`，注册进 `src/ecs/core/DefaultSystems.cpp`（照 `:136`）。
- `TerrainBuildSystem` 内实现**纯函数** `BuildTileList(view, lod_policy, tiles_out)`
  —— 这是全方案最重要的一条工程纪律：**T4 只是把这个函数原样搬进 compute**，
  算法与记录布局一行不改（对应引擎"单一真源"惯例）。判定逻辑不许分散到别处。
- 采样越界处理：tile 的 texel 窗口之外按边缘 clamp（对应源工程的分块重叠语义，但不再需要预复制数据）。
- 页/缓冲：`CreateSSBO` 建高度页缓冲；页表（T1-T3 可为单页）与 tile 表均由
  `GlobalSSBOBufferRegistry` 申请（§2.11），地址经 §3.4-A 的 `pc_root` 下发。
- **不接入通用 4-ID 批处理**：地形有自己的管线与命令面，避免与 `PrimitiveBatchPipeline` 的
  meshlet 双轨纠缠（`doc/meshlet-geometry-pipeline-implementation.md` §6.3）。

**T2 验证**：`TerrainTiles` 示例（同目录），判据 = 8×8 tile 无缝拼出完整地形，
且 tile 间无重复/空洞（用 tile 边界高亮调试材质或 overlay 检查）。
CPU 侧自检：相邻 tile 共享边上的 texel 原点差 == 期望值。

### T3 — LOD 与无缝验证

- `BuildTileList` 引入 LOD：按相机距离/屏幕空间误差选择 `lod`（误差度量用 tile `height_min/max`
  极差 + 屏幕投影尺寸）。
- 构建 **per-tile min/max 高度金字塔**（四叉树）：供 LOD 误差度量与地平线遮挡剔除
  （**注意：这不是高度图的 mipmap**，是高度界金字塔；见 §3.3 与决策表）。
- 落实 §3.3：`<<lod` 步长（顶点级重合）、对角线约定统一、**裙边**（裂缝兜底）；
  裙深取 `k·(height_max − height_min)`，k 由自检实测的裂缝深度反推。
- **T3 验证（两条判据，均可自动判定）**：
  1. **顶点级重合**：相邻粗细 tile 共享边上所有粗格点位置，用细网格插值高度与粗网格顶点高度比较，
     差值必须为 0（浮点严格相等或 < 1e-5）；
  2. **裂缝量化**：沿共享边密集采样 `|粗表面高度 − 细表面高度|` 取最大值 → 断言
     `skirt_depth ≥ 该最大值`。这条把"看起来没缝"变成可测的数字；
     边界截图作为辅助证据。

### T4 — GPU-Driven（compute 生成 tile 表 + 间接命令 + count）

- compute pass 写：tile 表 + `VkDrawMeshTasksIndirectCommandEXT[]` + count；
  count buffer 用 `CreateDrawCountBuffer`（`VKDevice.h:384-391`），
  挂 `MaterialBatch::icb_count_buffer`（照 `example/Basic/ComputeIndirectCount.cpp`）。
- 命令数 = tile 数；`groupCountX = ceil(cells/8)²`，`groupCountY/Z = 1`。
  如果将来一个 tile 内需要多级分辨率，才需要 `groupCountY`。
- **三条硬约束**（§2.9 已确认引擎不代管）：
  1. count 必须每帧先归零（`vkCmdFillBuffer` 或 compute 内专用 workgroup），
     且"归零 → append → 消费"三个动作的批次顺序必须正确；
  2. tile 表/命令数组的缓冲需 `STORAGE_BUFFER | INDIRECT_BUFFER`（+ tile 表若 BDA 读需
     `SHADER_DEVICE_ADDRESS`）；
  3. **tile 表与命令数组按帧多份化**（环形 2-3 份），否则本帧 compute 的写入会被上一帧
     仍在执行的绘制读到（`doc/backlog.md` A1 明确 per-frame 多份化未做）。
- **T4 验证**：渲染结果与 T3 **逐像素一致**（同相机、同 LOD 策略）；
  CPU 侧统计"每帧 CPU 提交耗时/绘制调用数"应降为 O(1)；
  用 `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` 跑一遍 grep `VUID|error`。

### T5 — 后续（不在本方案范围，列出以免架构走死）

- 大世界精度：接入 camera-relative（先完成 `TransformAssignmentBuffer::SetCameraOffset`，
  见 `Camera.cpp:64-65`）。
- 页流式：磁盘页加载 + `resident` 标志 + 非驻留页面的降级（跳过或降 LOD），
  与 `CMAssetsManage` 对接。
- 高度生成进 compute（正弦环/噪声都可以，替代源工程的 CPU 循环）。
- 可选：单一世界高度图 + 稀疏驻留（那时才评估 sparse texture；届时格式必须可过滤）。

---

## 5. 验证矩阵

| 功能路径 | 验证 target | 判据 |
|---|---|---|
| MeshShader 编译 + 新模式注册 | `TerrainBasic` | 管线创建成功、无 `#error mesh shader template missing`；GLSL 编译无告警 |
| Mesh 阶段 BDA 读高度 | `TerrainBasic` | 出图高度与 CPU 侧同索引值一致（同索引抽 10 个点比对） |
| 边缘组紧凑输出 | `TerrainBasic`（非 2 的幂 grid_dim，如 127/100 格） | 无空洞/无越界（validation layer 零 error；网格无破面） |
| tile 拼接 | `TerrainTiles` | 8×8 tile 拼合无重复/空洞；共享边 texel 原点自检通过 |
| LOD 接缝 | `TerrainLod` | 共享边粗格点高度差 == 0；**实测裂缝深度 ≤ skirt_depth**；边界截图无突变 |
| LOD 误差度量 | 同上 | 相机拉远后 tile 数下降、三角数下降（统计日志断言） |
| 间接绘制 / GPU count | `TerrainGpuDriven` | 与 T3 逐像素一致；绘制调用数 O(1) |
| 剔除 | 同上 | 视野外 tile 不产生 `groupCountX > 0` 的命令 |
| 回归（不破坏既有路径） | `SimpleMeshTriangle`、`TextDrawTest`、`LineRenderTest`、`LoadGeometry`、`CascadeShadowMap` | 全部照常出图；`PushRootAddresses` 三个调用点（`LineRenderPipeline:762`/`PipelineMaterialRenderer:173`/`TextRenderPipeline:290`）语义不变 |

---

## 6. 风险与开放问题（每条给出默认选择）

| # | 风险 / 问题 | 默认选择 |
|---|---|---|
| **R1** | `PushRootAddresses`（`inc/hgl/graph/RootAddressPush.h:28-39`）是 **11 个位置参数**的裸签名，加参数漏改某个调用点会**静默错位**（引擎已踩过同类坑：`ShaderResourceSchemaRegressionGate` 的参数错位） | **先结构体化再扩字段**：`struct RootAddressSet{VkBuffer/uint64 ...;}` + 单参重载，三个调用点迁移完再 T1。若不愿，至少加一个 `static_assert`/编译期校验并逐点核对 |
| R2 | `max_invocations` / 容量在不同设备上下限不同 | 复用现有设备钳制路径（`MeshShaderLimits.h` / `GenericMaterialBuilder` 的 CharQuad 分支模式），T1 就做 |
| R3 | LOD 级差超过 1 时（跳级）边界仍无缝吗 | §3.3 的条件对任意 2 的幂级差成立（子集性质），但**先按相邻级 ±1 实现并在 T3 自检里覆盖跳级用例** |
| R4 | `uint16` 高度的世界单位换算精度 | 保留"整数高度 + 单一 `height_scale`"（源工程语义），`height_scale` 为 float、在最后一步相乘 |
| R5 | 页与 tile 边界不对齐导致的跨页采样 | T2-T4 约束为"页内整数子矩形、tile 不跨页"；跨页支持留到 T5 |
| R6 | 高度界金字塔（T3）的实现位置 | 先 CPU 侧构建（简单、可自检）；T4 后随 tile 表一起进 compute 的候选，但不是必须 |
| R7 | 是否复用 `PositionSourceSpec::TerrainHeightmapGrid`（`PositionSourceSpec.h:12`）接入通用批处理 | **不复用**（专用管线更直接）。若希望地形统一走 4-ID 批处理，需先讨论（会与 meshlet 双轨分流耦合） |
| R8 | 碎片/调试：地形材质的片元配色 | T1 先用纯色 + 导数法线，T2 起再讨论高程着色/材质 splatting（那时才需要材质纹理的 mip） |

---

## 7. 回滚策略

- 每阶段结束打 tag：`terrain-t1` / `terrain-t2` …，回滚 = `git reset --hard <tag>`。
- **不可逆点只有一处**：`RootAddresses` 扩容（全局 push constant 结构变化）。
  在 T1 落地前先打 `pre-terrain-rootaddresses` tag；若 T1 验证失败，回滚该提交即可
  （两侧均由宏列表生成，不存在"改一半"的中间态文件）。
- 新增文件全部是新路径，不覆盖既有文件；对既有文件的改动集中在 §4-T1 的 8 个触点，
  每个触点都是加法（新增枚举/新增分支/新增字段），不存在删除语义。

---

## 附录 A：关键文件索引

| 作用 | 路径 |
|---|---|
| Mesh 模式枚举 / 解析 / 容量 | `inc/hgl/mtl/MeshShaderMode.h` |
| 模式描述符注册表 | `src/ShaderGen/meshgen/MeshModeDescriptor.h` |
| CharQuad（地形模式的骨架参照） | `src/ShaderGen/meshgen/MeshShaderModeCharQuad.h`、`ShaderLibrary/mesh/char_quad.glsl.tmpl` |
| BDA 契约（单一真源） | `inc/hgl/graph/ShaderBufferSources.h` |
| 根地址 push | `inc/hgl/graph/RootAddressPush.h` |
| 全局 SSBO 池 | `inc/hgl/graph/module/GlobalSSBOBufferRegistry.h`、`inc/hgl/graph/ssbo/SSBOTypes.h` |
| 专用管线参照 | `inc/hgl/ecs/support/line/*.h`、`src/ecs/support/line/*.cpp`、`src/ecs/core/DefaultSystems.cpp:136` |
| 间接绘制 / GPU count 参照 | `example/Basic/ComputeIndirectCount.cpp`、`src/ecs/support/PipelineMaterialRenderer.cpp:52-65` |
| 导数法线 NTB 模块 | `ShaderLibrary/ntb/ntb_derivative_normalmap.glsl` |
| 材质 TOML 自持模式参照 | `ShaderLibrary/material/text_2d_gpu.material.toml` |
| 剔除基础设施 | `example/Basic/ComputeFrustumCull.cpp`、`src/ecs/support/TestBoundingVolumeCull.cpp` |
| 帧资源在途现状 | `doc/backlog.md` A1 |

## 附录 B：术语

| 术语 | 含义 |
|---|---|
| 页（page） | 高度数据的流式单位，固定 texel 尺寸，落在高度缓冲的一段整数偏移上 |
| tile | 绘制单位，一个 grid_dim×grid_dim 顶点网格 + lod；"画在哪/读哪"的唯一权威 |
| 格（cell） | tile 内一个四边形，对应 mesh 阶段一个线程（4 顶点 + 2 图元） |
| 组（workgroup） | 64 线程，覆盖 8×8 格 |
| 粗/细 LOD | 相邻级的网格步长差 2 倍；粗格点坐标是细格点的子集 |
| 高度页缓冲 | 存放 `uint16` 高度的 BDA storage buffer（非纹理） |
| 高度界金字塔 | 每层 tile 的高度 min/max 树，用于剔除与 LOD 误差度量（**不是** mipmap） |
