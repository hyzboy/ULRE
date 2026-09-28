# GPU-Driven 地形渲染实现方案（MeshShader / BDA / LOD）

> ⚠ **本档已被取代**：v1（2026-09-24）基于已过期的引擎基线（`SCENE_SET`+`BINDLESS_SET` 双集、
> Camera/Viewport/Shadow 走 UBO 绑定、`pc_root` 72B），且其中「LOD 接缝不需要裙边」的结论**已被推翻**。
> 实现请以 **`doc/terrain-implementation-handbook.md`**（2026-09-28 重组的实现手册，唯一开工依据）为准；
> 推导过程在 `doc/terrain-implementation-plan-v2.md`；本档保留作历史与源工程细节索引。
>
> **2026-09-28 第二批口径变更（本档正文未逐处重写，读到冲突处一律以 v2 为准）**：
> ① 不走 **Task Shader**，改 **ComputeShader + MeshShader**；
> ② 阶段梯 T2 拆成四步（多 tile → LOD → **外扩重叠遮缝** → ICB 化），开发期以稳定性为准、不追性能；
> ③ **遮缝由「向下挤出裙边」改为「与邻居外扩重叠」**：预算 117/192 → **≤100/≤162**，
>    并**删掉 `skirt_depth` 与裂缝深度标定**（判据改为：无孔洞 + 平坦区无 z-fighting + 关掉外扩能复现裂缝）；
> ④ 高度/tile 表地址**改走 `pc_root`（D1-B，5 个新字段：3 地址 + 1 索引 + 1 补齐 ⇒ 80B → 112B）**，不再进全局地址表；
> ⑤ 对齐与容量约束、以及「高度走 buffer 不是纹理」的理由见 v2 **§2.6**。

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

> ⚠ **先读 §0.1**：引擎基线在 2026-09-28 已变（唯一 `Bindless(0)` 集 / 相机走 BDA / `pc_root` 80B /
> 全局地址表按 8 帧槽分份）。本表与下文凡与 §0.1 冲突处，**以 §0.1 为准**。

| 议题 | 决策 | 依据 |
|---|---|---|
| 网格生成 | MeshShader 内由索引生成 XY，Z 取高度场 | 无 VBO/EBO，CPU 只提交 tile 记录 |
| 顶点数据源 | 无顶点缓冲（`VertexInputMode::None` 同款自持路径） | CharQuad 模式已有先例 |
| 高度存储 | **BDA storage buffer**（非 texture、非 sparse） | mesh 阶段无法访问 bindless 纹理集；引擎全 BDA |
| 分块 | 两级：**页**（流式单位）+ **tile**（绘制单位） | 避免 tile 跨页两级间接 |
| Texture2DArray | 不用 | 规范下限 **256** 层，但**本机 `maxImageArrayLayers = 2048`**（1024 tile 其实放得下）⇒ 层数不是硬理由；硬理由是 **mesh 阶段读不到纹理（E7）**、整数组共享一条 mip 链无法按层流式、以及本方案本就不需要 mip（§2.4） |
| Sparse texture | **不做**（远期可选） | 引擎零 sparse 支持；tile 表已承担虚拟化 |
| Mipmap | 高度图**不做 mip**；做 per-tile min/max 金字塔 | mip 破坏 LOD 无缝前提；远景开销在顶点数不在采样 |
| 参数传递 | 整数域贯穿到**算出缓冲偏移**为止 | 浮点仅用于最终坐标/高度 |
| 世界坐标精度 | CPU double + tile 局部小浮点 | camera-relative 尚未启用（`Camera.cpp:64-65`） |
| 绘制路径 | 专用 `TerrainRenderPipeline`（照 `LineRenderPipeline` 四件套） | 不改通用 4-ID 批处理 |
| 裂缝处理（LOD 接缝） | 2 的幂步长保证**顶点级重合**；边内部 T 型缝用**外扩重叠**覆盖（2026-09-28 改；原"向下裙边"已弃用） | 见 §3.3 与 v2 §5.2 |
| 远景降载 | 2 的幂 LOD（步长翻倍 → 顶点数 1/4） | 顶点/图元数才是瓶颈 |

---

## 0.1 基线更新（2026-09-28）：S1/S2/S3 地址与描述符收敛

> 本文档首版写于 2026-09-24（当时基线 = `SCENE_SET`+`BINDLESS_SET` 双集 / Camera·Viewport·Shadow
> 走 UBO 绑定 / `RootAddresses` 72B）。此后引擎在另一条工作线上完成了**全局地址统一 S1/S2/S3 +
> Scene 集退场**（`doc/global-addresses-bda-unification-plan.md`、`doc/scene-ubo-bda-migration-handoff.md`），
> 契约已变。下表是差异清单与本文档的修订口径：**下游各节凡与本表冲突，以本表为准。**

| 契约 | 首版基线（已过期） | 现基线（2026-09-28 实测） | 出处 |
|---|---|---|---|
| 描述符集 | `SCENE_SET=0` + `BINDLESS_SET=1` | **唯一集合 `BINDLESS_SET=0`**；Scene 集整体退场（44 文件 +132/−1285） | `e59e65620`、`ShaderLibrary/common/descriptor_macros.glsl` |
| 相机数据 | Scene 集 Camera UBO 绑定 | **BDA 宏、无绑定**：`#define camera CameraInfoBufferRef(global_addresses.addr_camera_info).cameras[pc_root.camera_id]` | `86535dbdb`、`ShaderLibrary/ubo/scene_ubo.glsl:149` |
| viewport / sky / shadow | Scene 集内 UBO 绑定 | 地址进全局地址表，**宏名与成员名不变** ⇒ 读点零改动 | `57cb02db6` / `067ce9e3f` / `24b2b785d` |
| push descriptor | 有（viewport 等路径） | **整条路径已删**（函数指针 + 设备特性 + 使用标志） | `e59e65620` |
| `pc_root` / `RootAddresses` | 72B = 8×uint64 + 2×uint32 | **80B = 9×uint64 + 2×uint32**，首字段 `addr_global_addresses`；布局断言**按 X 列表自动推导** | `7290a9207`、`ShaderBufferSources.h:267-278` |
| 全局地址表 | 64B UBO、Set 0 binding 2 | **SSBO、无绑定无集**：基址 `pc_root.addr_global_addresses`，按 **`HGL_FRAME_SLOT_TOTAL=8` 帧槽**分份（槽步长 128B、16B 对齐） | `7290a9207`/`966fb3d7e`、`inc/hgl/graph/ubo/GlobalAddresses.h` |
| 地址归口口径 | —（隐含"pc_root 一张表装全部"） | **全局/长期有效 → 全局地址表（按帧槽）；每批/每材质/每字体 → `pc_root`**（同帧内逐批不同，标量表装不下） | `GlobalAddresses.h:18-20`、`RootAddressPush.h:31-43` |
| 帧槽划分 | — | `HGL_FRAME_SLOT_TOTAL=8`，**主帧槽 `[0,HGL_FRAME_SLOT_MAIN=4)` 与离屏槽 `[4,8)` 不相交**；ring 深度必须 = 槽总数（小于会别名覆写） | `inc/hgl/common/RenderOptions.h:16-33` |
| 材质声明描述符 | `ubos=[...]` 可声明 | **硬不变量：definition 声明的描述符必须为空**（TOML `ubos` 键、授权规则表、`ubo_requirements` 全删） | `e59e65620` |
| 新增门 | — | `S.global-addresses-struct-parity`（C++ 表结构 ↔ GLSL `GlobalAddressesRef` 同序同型），门基线 **39 PASS / 0 FAIL** | `243d2d5ff` |
| 上传/内存 | 逐次 staging 分配 + CPU memcpy | **环形 Staging 池**（双队列各 16MB、256B 切片、>8MB 降级独立缓冲）+ UMA 零拷贝（`TRANSFER_SRC` 锁 `CPUVisible`） | `4170b23a9`/`61fc1ed90`、`doc/async-texture-upload-and-bindless.md` |

### 0.1.1 对本地形方案的直接影响（四条）

1. **地形表地址的落位变了**（原 §3.4 的 A/B 要按新口径重读）：`pc_root` 现在是"1 张表基址 + 本批/每材质地址"，
   不再是"8 张表地址"。地形两张表可走 **全局地址表**（见 §3.4-A）或 **`RootAddresses`**（见 §3.4-B）。
2. **相机矩阵不需要任何绑定**：`camera` 宏含 `vp` / `inverse_vp` / `frustum_planes[6]` / `pos` /
   `use_reversed_z` —— 地形剔除与 reversed-Z 所需字段全在，mesh/task 阶段直接可用。
3. **在途帧问题有了引擎机制**（原 §2.9 的"引擎不代管"已部分失效）：照 L2W / CameraInfo / shadow 的样，
   地形的每帧缓冲做成 `HGL_FRAME_SLOT_TOTAL` 深度的 ring 并按帧槽取地址；**深度小于槽总数会别名覆写**。
4. **三条硬规矩**（S2/S3 用事故换来的）：
   - 地址必须在 **buffer 物化处**注册（只在"每帧同步函数"里注册 ⇒ 某路径没跑到 ⇒ 地址恒 0 ⇒
     shader 解引用 0 基址 ⇒ `vkQueueSubmit2 failed with result -4` = **设备丢失**，校验层只报次生错误）；
   - **C++ 表结构与 GLSL `GlobalAddressesRef` 必须同批改**（S2d 漏一行 ⇒ 材质静默回退默认材质：
     画面照常有内容、门全绿、0 校验层 —— 教训见 `global-addresses-bda-unification-plan.md` §2.3）；
   - 每个渲染门**单独 grep `result -4`**（校验层消息数 0 也可能是崩完之后的次生状态）。

### 0.1.2 与 `doc/Terrain Vulkan 1.4.rtf` 的差异与裁决

`doc/Terrain Vulkan 1.4.rtf`（2026-09-24，Task+Mesh / 81 顶点拓扑 / 相机相对）是**更新的设计意图**，
冲突处按其口径执行；但该文档有两处**与 2026-09-28 的引擎基线冲突**，必须改：

| RTF 写法 | 问题 | 裁决 |
|---|---|---|
| `layout(set = 0, binding = 0) uniform CameraData { mat4 relative_vp; ... } u_cam;` | Scene 集已退场、Camera UBO 已删（`86535dbdb`）；且**材质定义声明描述符已违反硬不变量** | 删绑定，改用 `camera.vp` / `camera.pos` 宏（BDA，零绑定） |
| §0「Push Descriptors + 精简 PC ≤32B」 | push descriptor 路径已整体删除 | 保留"精简 PC"目标，但**不能**用 push descriptor；相机走 `camera` 宏后 PC 里本就不需要 `mat4` |
| PC 里直接放 `addr_heights` / `addr_tiles`（私有 PC 布局） | `pc_root` 是引擎统一结构（`camera` 宏必需的 `camera_id` + `addr_global_addresses` 都在里面），私有 PC 会丢根入口 | 两选一：**(a) 进全局地址表**（§3.4-A，本档推荐）；**(b) 加进 `RootAddresses`**（§3.4-B） |
| 9×9 = 81 顶点对应 8×8 格；`max_vertices=128, max_primitives=192`；外扩重叠后最坏 10×10 ⇒ ≤100 顶点 / ≤162 图元 | 与本文档原"每线程 1 格 = 4 顶点"冲突 | **采纳 RTF**：顶点复用把 8×8 格的顶点数 256→81；117 ≤ 256、192 ≤ 256 —— **NV 的 256 下限设备同样成立** |
| Task Shader 原生级联分发（替代 compute+indirect+count） | 引擎有 stage 级管道（`ShaderStageDef.h:11` 已定义 Task 位，GLSLCompiler 映射 `task`），但 meshgen **没有 task 阶段发射** | 采纳为方向，成本计入 §4-T4：meshgen 需新增 task stage 发射；compute+indirect 作为可回退路径保留 |
| 高度用原生 `uint16_t[]`（`storageBuffer16BitAccess`） | 与本文档原"uint32 装 16 位"不同 | **采纳 RTF**（省一半带宽）；本机实测 `storageBuffer16BitAccess=true` / `shaderInt16=true`（该路径可走），T1 启动仍应断言 |
| 法线在 mesh 阶段中心差分 | 本文档原为片元 `dFdx/dFdy` | **采纳 RTF**（低模无刻面感，且省一次法线贴图） |

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
| **`RootAddresses`** | **80B**（9×uint64 + 2×uint32，首字段 `addr_global_addresses`） | `:267-278`，布局断言按 X 列表自动推导 | push constant：**表根入口 + 每批/每材质地址 + `camera_id`** |

全部由 `HGL_*_FIELD_LIST` 宏列表单源生成（CPU 成员 / GLSL 字段名 / GLSL 类型 / 布局断言），
GLSL 侧由 `MeshShaderHeaderGen.h:29+`（mesh 阶段）与 `MaterialShaderEmitter.cpp:512+`（片元阶段）遍历发射。

### 2.4 地址承载的两条既有通路（地形表落位见 §3.4）

**(a) 全局地址表 `GlobalAddresses`（S1/S2 后的主通路）** —— SSBO、无绑定无集，
基址 = `pc_root.addr_global_addresses`，表内每字段一个 64 位地址：

```glsl
// ShaderLibrary/ubo/scene_ubo.glsl:102-119
layout(buffer_reference, scalar, buffer_reference_align=16) readonly buffer GlobalAddressesRef
{
    /* …8 个全局字段（含 addr_camera_info）… */
    uint64_t addr_sky; uint64_t addr_viewport; uint64_t addr_shadow;   // 每帧槽字段
};
#define global_addresses GlobalAddressesRef(pc_root.addr_global_addresses)
#define camera CameraInfoBufferRef(global_addresses.addr_camera_info).cameras[pc_root.camera_id]
```

**(b) `pc_root` 直挂（每批 / 每材质 / 每字体）** —— 文本三表就是先例
（`ShaderLibrary/vertex/s1_text_char_quad.glsl:45`）：

```glsl
layout(buffer_reference, scalar, buffer_reference_align=16) buffer TextCharInfoRef { TextCharInfo chars[]; };
#define sbo_char_info TextCharInfoRef(pc_root.addr_text_char_info)
```

**归口口径**（`GlobalAddresses.h:18-20`）：全局 / 长期有效 → (a)；同帧内逐批不同 → (b)。
地形两张表两条路都合法，选法见 §3.4。

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

### 2.7 限制一：mesh 阶段**不能**采样 bindless 纹理（2026-09-28 复核：仍成立）

> 复核：`VKBindlessTextureManager.cpp:42/48/54`（另有 `:220/226/232`）六个 binding 的 `stageFlags`
> 仍是 `FRAGMENT|COMPUTE`；集合号已收敛为 `BINDLESS_SET=0`，且 bindless 里**只剩纹理 / Cube / 采样器**
> （viewport / sky / shadow / camera 已全部走 BDA）。

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

### 2.9 在途帧：引擎已有"按帧槽分份"机制（首版结论已过期）

- 全局地址表按 `HGL_FRAME_SLOT_TOTAL=8` 切 8 槽；CameraInfo 按「相机序号 × 槽」分份
  （行号 = `camera_id*8 + 槽`）；shadow ring 一槽一份；L2W ring 深度 = 槽总数（`RenderOptions.h:33`）。
- **但** Camera / Viewport 仍是**单份 buffer + 内容覆盖写、地址恒定**（见
  `global-addresses-bda-unification-plan.md` §2.2d 的更正），`backlog.md` A1 的口径要按这条修正理解。
- 对地形的结论：**tile 表 / 间接命令数组 / count 做成 `HGL_FRAME_SLOT_TOTAL` 深度的 ring 并按帧槽取地址**
  （照 L2W / CameraInfo 的样）。**ring 深度 < 槽总数会别名**：离屏槽覆写主帧在途数据，
  症状是"整帧只剩清屏色"（`RenderOptions.h:24-33` 记录的事故）。

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

> **更正**：本方案早期版本曾断言"2 的幂步长 + 双线性采样即可无缝、不需要裙边"——**该结论错误**。
> **再次更正（2026-09-28）**：遮缝手段已定为**外扩重叠**（不是向下裙边），预算与判据全部改变，
> **以 v2 §4.1/§5.2 为准**；下面这几段只保留"为什么必须有遮缝手段"的论证。

**T 型交点缝是固有几何问题，不能靠采样方式消除。**

- 相邻 LOD 共享一条边时：细侧是**折线**（穿过多个顶点），粗侧是**弦**（只连两端点）。
  两者只有在该边上的高度函数为线性（平坦或恒定坡）时才重合。
- "2 的幂步长 + 同一高度函数"能保证的是**顶点级重合**：粗格点位置是细格点子集 →
  取到同一个 texel → 同一个高度值。这**不等于**边内部重合。

三种工程手段：

| 手段 | 说明 | 代价 | 本方案 |
|---|---|---|---|
| **a. 外扩重叠（现行方案）** | 每个 tile 的网格向 −X/−Y 各多生成 1 格，与邻居几何重叠 ⇒ 缝被**覆盖** | 顶点 81→100、图元 128→162；**无第二套索引逻辑、不需要深度参数** | **T2.3 采用（v2 §5.2）** |
| b. 向下裙边（skirt） | tile 边缘额外向下挤出一圈三角形把缝"堵住" | 独立索引逻辑 + 绕序 + 裙深参数 + **需按裂缝深度标定** | **已弃用**（2026-09-28） |
| c. 顶点形变（geomorph） | 细侧过渡带顶点向粗侧弦插值 | 需 morph 因子与过渡带判定 | 可选优化，不进开发期 |
| d. 过渡带缝合 | 粗 tile 外圈改用细步长 | tile 需知邻居 LOD，网格不再均匀 | 不采用 |

**双线性采样的真实作用**：把"高度函数"与"网格分辨率"解耦（网格点落在 texel 之间时避免阶梯），
并让不同 LOD 在任意位置取到同一函数值——这使裂缝**有界且可量化**（是 a/b 的前提）。
在"2 的幂 + texel 对齐"时它退化为直接取纹素（fraction = 0），因此**它不是消裂手段**。

**~~裙深怎么定~~（已删除）**：外扩重叠的 ε 只需要"避开与邻居表面共面"，**不需要覆盖裂缝深度**
⇒ 不需要任何裂缝深度测量与标定，也不需要上面那两种取法。判据见 v2 §5.2 三条：
边界带无孔洞 / 平坦区多帧截图无 z-fighting / **关掉外扩（`flags` 位0）能复现裂缝**。

**对角线约定**：格内固定按 `(TL,BL,TR)+(TR,BL,BR)` 或 `(TL,TR,BR)+(TL,BR,BL)` 之一对角化，
全局一致（不一致只造成亚 texel 级差异，但保持一致成本为零）。

**LOD 级差约束**：网格步长必须为 2 的幂（粗格点坐标 ⊂ 细格点坐标）——主流地形方案的通行约束，
本方案沿用。它保证顶点级重合，把裂缝压到"可量化的最小"。

### 3.4 数据注入通路（新基线上的两条路）

前置：`pc_root` 已是"1 张表基址（`addr_global_addresses`）+ 每批/每材质地址 + `camera_id`"；
`camera` 宏经全局地址表取相机行 ⇒ **地形不需要任何描述符 / 绑定**。

**A（推荐）：进全局地址表 `GlobalAddresses`。**
- 高度页缓冲（静态、长期有效）→ 加进**全局字段段**（`addr_terrain_heights`，一行）。
- tile 表（每帧重建）→ 加进**每帧槽字段段**（`addr_terrain_tiles`），与 `addr_shadow` 同形
  （那个就是"每帧槽一份"的现成先例：`ring[i] ↔ 帧槽 i`）。
- 收益：`pc_root` 不变大（仍 80B）、地址天然按帧槽安全、shader 侧只多两行宏
  （`#define terrain_heights TerrainHeightsRef(global_addresses.addr_terrain_heights)`）。
- 成本：动 `GlobalAddresses` ⇒ **C++ 与 GLSL `GlobalAddressesRef` 必须同批改**
  （门 `S.global-addresses-struct-parity` 会守；记住 S2d 教训：漏一行的症状是**材质静默回退**）。

**B（备选 → 2026-09-28 已定案选 B）：加进 `RootAddresses`（`pc_root`）。** 与文本三表完全同构：
`HGL_ROOT_ADDRESSES_FIELD_LIST` 追加 **5** 行（`addr_terrain_heights` / `addr_terrain_tiles` / `addr_terrain_frame`
+ `terrain_tile_index` + `_pad_terrain`），`pc_root` 80B → **112B**（规范保证下限 128B，仍安全；尾部 u32 必须成对补齐）。
> 定案理由与直发期的 per-draw tile 索引有关；**GLSL 侧 `pc_root` 块由生成器按同一 X 列表发射**
> （`MeshShaderHeaderGen.h:28`、`MaterialShaderEmitter.cpp:510`），**没有手写的 GLSL 块要同步改** —— 见 v2 §3.1/§6.2 触点 8。
- 收益：地址随命令缓冲自带时序，**不涉及帧槽语义**；适合将来"每 tile 一个基址"的形态。
- 成本：每批次多推 16B；且要动 `PushRootAddresses` 的位置参数（见 §6-R1）。
- 布局断言已自动化（按 X 列表推导），首版担心的"手列下标静默过期"已被引擎修掉。

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
| 8 | 走 A：`inc/hgl/graph/ubo/GlobalAddresses.h` + `ShaderLibrary/ubo/scene_ubo.glsl:102-117`；走 B：`inc/hgl/graph/ShaderBufferSources.h:267-278` + `RootAddressPush.h:31-43` | 加地形两表地址。**A**：C++ 表 + GLSL `GlobalAddressesRef` 同批改（门 `S.global-addresses-struct-parity` 守）；**B**：X 列表加两行 + `PushRootAddresses` 加参（**先做 §6-R1 的结构体化**） |

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
- 落实（**按 v2 §4.1/§5.2 的新口径**）：`<<lod` 步长（顶点级重合）、对角线约定统一、
  **外扩重叠遮缝**（网格格域 = `8N+1` 格/边，外扩带高度 −`overlap_epsilon × 格世界尺寸`，
  `flags` 位0 可关闭作对照）。
- **T3 验证（两条判据，均可自动判定）**：
  1. **顶点级重合**：相邻粗细 tile 共享边上所有粗格点位置，用细网格插值高度与粗网格顶点高度比较，
     差值必须为 0（浮点严格相等或 < 1e-5）；
  2. **遮缝有效性（成对判据）**：边界带无孔洞；平坦区多帧截图一致（无 z-fighting）；
     **关掉外扩能复现可见裂缝**。只报"开着没缝"不构成证据（无法区分"遮住了"与"本来就没缝"）。

### T4 — GPU-Driven（两条路：~~Task Shader 原生分发~~ / compute 生成间接命令）

> **2026-09-28 定案**：**Task Shader 路线取消**，只走 compute 生成 tile 表 + 间接命令（v2 §4.2/T3）。

> **路 1（`doc/Terrain Vulkan 1.4.rtf` 的方向，采纳）**：Task Shader 每 workgroup 决策一个 tile，
> `EmitMeshTasksEXT(N,N,1)` 原生派发，剔除失败发 `EmitMeshTasksEXT(0,0,0)` ⇒ CPU 只发一次
> `vkCmdDrawMeshTasksEXT(tile_count,1,1)`，**不需要 indirect / count / 归零链路**。
> 引擎侧成本 = meshgen 新增 task stage 发射（`ShaderStageDef.h:11` 已定义 Task 位）。
> **路 2（可回退，API 已就绪）**：compute 写 tile 表 + 间接命令 + count。

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
  3. **tile 表与命令数组按 `HGL_FRAME_SLOT_TOTAL`（=8）深度的 ring 分份、按帧槽取地址**
     （§2.9）。**深度小于槽总数会别名覆写**，症状 = 整帧只剩清屏色。
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
| LOD 接缝 | `TerrainLod` | 共享边粗格点高度差 == 0；未遮缝时**预期可见裂缝**（截图存档）；遮缝后按 v2 §5.2 三条判 |
| LOD 误差度量 | 同上 | 相机拉远后 tile 数下降、三角数下降（统计日志断言） |
| 间接绘制 / GPU count | `TerrainGpuDriven` | 与 T3 逐像素一致；绘制调用数 O(1) |
| 剔除 | 同上 | 视野外 tile 不产生 `groupCountX > 0` 的命令 |
| 地址表改动（走 §3.4-A 时） | `ShaderResourceSchemaRegressionGate` | `S.global-addresses-struct-parity` PASS（门基线 39 PASS / 0 FAIL）；材质未被静默回退默认材质 |
| 设备丢失排查（每次渲染门） | 任意地形 target | 单独 `grep "result -4"` 为空；出现 `addr_*=0` 警告即 fail |
| 帧槽正确性 | `TerrainGpuDriven` | 连续 ≥8 帧（跨主帧槽与离屏槽）无"整帧清屏色"；tile 表地址随帧槽变化且不回绕到在途槽 |
| 回归（不破坏既有路径） | `SimpleMeshTriangle`、`TextDrawTest`、`LineRenderTest`、`LoadGeometry`、`CascadeShadowMap` | 全部照常出图；`PushRootAddresses` 现有调用点（`LineRenderPipeline:772` / `PipelineMaterialRenderer:177` / `TextRenderPipeline:300`）语义不变 |

---

## 6. 风险与开放问题（每条给出默认选择）

| # | 风险 / 问题 | 默认选择 |
|---|---|---|
| **R1** | `PushRootAddresses`（`inc/hgl/graph/RootAddressPush.h:31-43`）是 **12 个位置参数**的裸签名——S1 把 `addr_global_addresses` 插到了第 4 位，**风险已应验一次**；再加 `addr_terrain_*` 就是第三次动它 | **先结构体化再扩字段**（`struct RootAddressSet{...}` + 单参重载），或**走 §3.4-A 完全不动这个签名**（这是推荐 A 的一条实际理由）。若都不做，至少逐调用点核对 |
| R2 | `max_invocations` / 容量在不同设备上下限不同 | 复用现有设备钳制路径（`MeshShaderLimits.h` / `GenericMaterialBuilder` 的 CharQuad 分支模式），T1 就做 |
| R3 | LOD 级差超过 1 时（跳级）边界仍无缝吗 | §3.3 的条件对任意 2 的幂级差成立（子集性质），但**先按相邻级 ±1 实现并在 T3 自检里覆盖跳级用例** |
| R4 | `uint16` 高度的世界单位换算精度 | 保留"整数高度 + 单一 `height_scale`"（源工程语义），`height_scale` 为 float、在最后一步相乘 |
| R5 | 页与 tile 边界不对齐导致的跨页采样 | T2-T4 约束为"页内整数子矩形、tile 不跨页"；跨页支持留到 T5 |
| R6 | 高度界金字塔（T3）的实现位置 | 先 CPU 侧构建（简单、可自检）；T4 后随 tile 表一起进 compute 的候选，但不是必须 |
| R7 | 是否复用 `PositionSourceSpec::TerrainHeightmapGrid`（`PositionSourceSpec.h:12`）接入通用批处理 | **不复用**（专用管线更直接）。若希望地形统一走 4-ID 批处理，需先讨论（会与 meshlet 双轨分流耦合） |
| R8 | 碎片/调试：地形材质的片元配色 | T1 先用纯色 + 法线（法线按 RTF 改 mesh 阶段中心差分），T2 起再讨论高程着色/材质 splatting（那时才需要材质纹理的 mip） |
| R9 | 地址口径选错（每帧变化的表放进"全局字段"段 ⇒ 在途帧被覆写） | 按 §3.4-A 分段落位：静态高度页 → 全局字段段；每帧 tile 表 → 每帧槽字段段（照 `addr_shadow`） |
| R10 | 地址为 0 被解引用 ⇒ **设备丢失**（`result -4`），校验层只报次生错误、门可能照常 PASS | 地址在 **buffer 物化处**注册 + 取不到即 fail-fast + 打 `addr=0x… 入表` 日志；每个渲染门单独 grep `result -4` |
| R11 | 走 §3.4-A 改表结构时漏改 GLSL 一行 ⇒ **材质静默回退默认材质**（画面照常有内容、门全绿） | C++ 与 GLSL 同批改（门 `S.global-addresses-struct-parity`）；渲染型门必须含 ATS / CSM 那类 |

---

## 7. 回滚策略

- 每阶段结束打 tag：`terrain-t1` / `terrain-t2` …，回滚 = `git reset --hard <tag>`。
- **不可逆点**（按 §3.4 的选路）：走 A ⇒ `GlobalAddresses` 结构变化（C++ + GLSL 同批，门守）；
  走 B ⇒ `RootAddresses` 扩容（全局 push constant 结构变化）。两者都应在 T1 落地前打 tag。
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
