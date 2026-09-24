# Vulkan 独立工程地形渲染实验方案（MeshShader / GPU-Driven / BDA）

> **定位**：为**独立 Vulkan 工程**（不依赖 ULRE）设计的地形渲染实验方案，从零搭建。
> 目标是把 `D:\AIProgramming\Terrain`（OpenGL 4.6，下文"源工程"）的地形技术用 Vulkan 重做，
> 并按 GPU-Driven 形态演进到 compute 生成间接绘制。
>
> **与 ULRE 版的差异**：另一份 `doc/terrain-rendering-implementation-plan.md` 受 ULRE 既有架构
> 约束（bindless 纹理集 stageFlags 无 mesh 阶段、相机相对渲染未启用、帧资源单份、mesh 模式需
> 注册进 ShaderGen）。独立工程**没有这些约束**——可以自由给描述符加 mesh 阶段、可以一开始
> 就按帧多份化、可以直接做纹理采样与稀疏驻留对照实验。本方案中标注「独立工程优势」之处即是。
>
> **本机环境实测**（Vulkan SDK 1.4.357.0，`C:\VulkanSDK\1.4.357.0\Bin` 有 `glslc` /
> `glslangValidator` / `spirv-val`）：
> GPU = Intel(R) Arc(TM) 140T GPU (16GB)，驱动 101.8991；
> 可读参考实现树 `D:\SaschaWillems\Vulkan\examples\`（含 `meshshader`、`indirectdraw`、
> `computecullandlod`、`bufferdeviceaddress`、`terraintessellation`）。
> 附录 A 的 GLSL 骨架已用本机 `glslc --target-env=vulkan1.3` 与 `spirv-val` 编译校验通过。

---

## 0. 决策速览

| 议题 | 决策 | 依据 |
|---|---|---|
| 网格生成 | MeshShader 内由索引生成 XY，Z 取高度场 | 无 VBO/EBO，CPU 只提交 tile 记录 |
| 顶点缓冲 | 完全不存在（mesh 管线无顶点输入阶段） | `pVertexInputState`/`pInputAssemblyState` 必须为 NULL |
| 高度存储 | **storage buffer + BDA**（`uint32` 装 16 位值起步） | 整数索引寻址；无采样器状态；见 §2.3 |
| 分块 | 两级：**页**（流式单位）+ **tile**（绘制单位） | 避免 tile 跨页两级间接 |
| Texture2DArray | 不用 | 层数上限；整数组共享 mip 链，无法按层流式 |
| 纹理路线 | 独立工程里**可行**（可给 binding 加 `VK_SHADER_STAGE_MESH_BIT_EXT`），列为 A/B 对照实验 | ULRE 版受限，本工程无此约束 |
| Sparse 稀疏驻留 | 硬件支持（本机 `sparseResidency*` 全 true），但**不作为第一阶段** | 工程量（HEAP 内存/page size/mip tail/`vkQueueBindSparse`）另计 |
| Mipmap | 高度图**不做 mip**；做 per-tile min/max 金字塔 | mip 改变几何函数；远景开销在顶点数不在采样 |
| 裂缝（LOD 接缝） | **裙边（skirt）为默认**；形变为可选优化 | 见 §3.3（T 型缝无法靠采样消除） |
| 参数传递 | 整数域贯穿到算出**元素索引**为止 | 浮点仅用于最终坐标/高度；`VkDeviceAddress` 是唯一 64 位出口 |
| 世界坐标精度 | CPU double + tile 局部小浮点（`shaderFloat64` 本机 = false） | shader 内不能靠 double 兜 |
| 绘制路径 | 单管线 + `vkCmdDrawMeshTasksIndirectCountEXT` | V4 一次提交全部 tile |
| 帧资源 | 一开始就按帧槽多份化（tile 表/命令/count） | 独立工程可直接做对 |

---

## 1. 源工程分析与评价

### 1.1 技术原理链（七环）

| 环 | 实现 | 位置 |
|---|---|---|
| 1 网格 | **无 VBO**，只有 EBO；顶点位置由 `gl_VertexID` 在 VS 内生成 | `terrain_grid.cpp:5-30`、`shaders/terrain.vert:11-33` |
| 2 高度场 | 径向正弦环，`R16UI` 16 位整数高度 | `terrain_procedural.cpp:8-32` |
| 3 分块 | `gridDim=8 / blockSize=128`，带 1 行/列重叠 | `terrain_procedural.cpp:89-110`、`terrain_manager.h:13-15` |
| 4 法线 | CPU 中心差分烘 `RGB8` 法线图 | `terrain_procedural.cpp:34-66` |
| 5 装载 | 两张 `texture2DArray`（高度 64 层 `R16UI` + 法线 64 层 `RGB8`） | `terrain_manager.cpp:15-23` |
| 6 绘制 | 64 次 `glDrawElements` + 每块平移矩阵 + `uTexLayer` | `terrain_manager.cpp:26-44` |
| 7 着色 | VS 用 `usampler2DArray`+`texelFetch` 位移；FS 贴法线图做单光照明 | `shaders/terrain.vert:21-33`、`shaders/terrain.frag:9-16` |

坐标系 Z-up（`camera.h:8-9`），新工程建议保持，与源工程一致。

### 1.2 已确认缺陷（复现时不要连 bug 一起搬）

1. **法线图块边界钳制 → 块间亮度缝**：`getH()` 把块外坐标 clamp 到边界
   （`terrain_procedural.cpp:38-42`），首/末行列偏导减半；重叠行列的数据已算好却未用。
2. **法线图 UV 半纹素/越界**：`vUV=(gx,gy)/(uSize-1)` + `GL_LINEAR/GL_REPEAT`
   （`terrain.vert:16-19`、`texture2d.cpp:73-76`），0/1 处跨到对边纹理。
3. **零剔除**：64 块每帧全画（`terrain_manager.cpp:33-41`），约 203 万三角/帧。
4. **无法独立缩放高度**：`uScale` 对 XYZ 同乘（`terrain.vert:29-30`）且模型矩阵只平移
   （`terrain_manager.cpp:36`），16 单位起伏铺在 ~1000 单位跨度上。
5. **工程性**：着色器相对路径加载（`main.cpp:67`）；`CMakeLists.txt:35` 引用不存在的 `${Stb_INCLUDE_DIR}`。

### 1.3 保留 / 替换

| 源工程 | 新工程 |
|---|---|
| `gl_VertexID` 生成 XY | 保留，改 MeshShader 索引生成 |
| 16 位整数高度场 | 保留，改 storage buffer + 整数索引 |
| 1 行/列重叠分块 | 替换：页偏移 + 按需越界采样 |
| CPU 法线图 | 替换：屏幕空间导数法线（`dFdx/dFdy`） |
| 整块一次 draw | 替换：1 tile = N 个 workgroup；V4 单次间接提交 |
| 64 层 Texture2DArray | 替换：页/tile 整数偏移 |
| 平移矩阵 + `uScale` | 替换：tile 世界原点（整数）+ 独立高度缩放 |

---

## 2. Vulkan 侧前提（**本机实测值**，非规范抄录）

### 2.1 扩展与特性

| 需要 | 本机实测 | 说明 |
|---|---|---|
| `VK_EXT_mesh_shader` | ✅ 存在 | 核心；`vkCmdDrawMeshTasksEXT` 家族 |
| `VK_KHR_draw_indirect_count` | ✅ 存在 | V4 的 `vkCmdDrawMeshTasksIndirectCountEXT` |
| `VK_KHR_buffer_device_address` / `bufferDeviceAddress` | ✅ feature true | BDA 必需 |
| `shaderInt64` | ✅ true | GLSL `uint64_t` 字段与地址表达 |
| `scalarBlockLayout`（Vulkan 1.2） | 需开 | `buffer_reference` + `scalar` 布局必需 |
| `shaderFloat64` | ❌ **false** | shader 内不能靠 double 保精度（见 §3.5） |
| `timelineSemaphore` / `multiDrawIndirect` / `drawIndirectFirstInstance` | ✅ true | 可选优化 |
| `sparseResidencyBuffer/Image2D/Image3D/Aliased` | ✅ 全 true | 硬件支持稀疏驻留（V5 可选，见 §6-R8） |

需要开启的结构：`VkPhysicalDeviceMeshShaderFeaturesEXT{meshShader=TRUE, taskShader=FALSE}`、
`VkPhysicalDeviceVulkan12Features{bufferDeviceAddress, scalarBlockLayout, drawIndirectCount}`、
`VkPhysicalDeviceFeatures{shaderInt64}`。
若用 16 位存储（省内存）另需 `storageBuffer16BitAccess` + `GL_EXT_shader_16bit_storage`。

### 2.2 设备上限（Intel Arc 140T / 驱动 101.8991）

| 上限 | 本机实测 | 规范保证下限 | 本方案占用 |
|---|---|---|---|
| `maxMeshOutputVertices` | **1024** | 256 | 256（64 线程 × 4 顶点） |
| `maxMeshOutputPrimitives` | **1024** | 256 | 128 |
| `maxMeshOutputMemorySize` | 524288 B | 32768 B | 远低于 |
| `maxMeshOutputComponents` | 128 | — | ≤ 16（位置 + UV） |
| `maxMeshWorkGroupInvocations` | 1024 | 128 | 64 |
| `maxMeshWorkGroupSize` | (1024,1024,1024) | (128,1,1) | (64,1,1) |
| `maxTaskPayloadSize` | 65504 | — | 未用（无 task shader） |
| `maxPushConstantsSize` | **256** | 128 | 104（见附录 A） |
| `maxStorageBufferRange` | 4294967292 | — | 高度缓冲 < 1 GB |
| `minStorageBufferOffsetAlignment` | 64 | — | 不适用（按元素索引寻址，见 §2.3） |
| `nonCoherentAtomSize` | 1 | — | flush 粒度无额外成本 |
| `bufferImageGranularity` | 1 | — | 缓冲/图像混放无障碍 |
| `timestampPeriod` | 52.0833 ns | — | 性能自检用 |

**⚠ 可移植性**：NV 系 `maxMeshOutputVertices` 通常是 **256**——本方案刚好卡在规范下限
（64 线程 × 4 顶点 = 256），所以**在 NV 上也能跑**；但不要再往上加（如 128 线程/组 → 512 顶点
会在 NV 上创建管线失败）。要加大组必须走"每线程 1 个格"的减法（如 32 线程 × 4 = 128）而不是加法。

### 2.3 存储与内存

```
高度缓冲   VkBuffer  STORAGE_BUFFER | SHADER_DEVICE_ADDRESS | TRANSFER_DST
           DEVICE_LOCAL，一次性 staging 上传（静态数据）
tile 表    VkBuffer  STORAGE_BUFFER | SHADER_DEVICE_ADDRESS     ← V1-V3 CPU 写
tile 表    VkBuffer  + INDIRECT_BUFFER                          ← V4 compute 写
命令数组   VkBuffer  INDIRECT_BUFFER | STORAGE_BUFFER | SHADER_DEVICE_ADDRESS
count      VkBuffer  INDIRECT_BUFFER | STORAGE_BUFFER | TRANSFER_DST   （1×uint32）
```

- **高度元素格式三选一**（本方案默认 A）：
  - **A. `uint32[]`，每 texel 一个 uint（低 16 位存高度）** — 索引最简单，内存 2×（1K² = 4 MB，无所谓）
  - B. `uint16[]` — 需 `storageBuffer16BitAccess`，省一半内存
  - C. `float[]` — 直接可用，精度对 ≤2²⁴ 的整数高度无损
- **寻址规则（重要）**：**一个基地址 + 整数元素索引**，不要"每 tile 一个 `VkDeviceAddress` 做指针算术"。
  前者无对齐坑（`buffer_reference_align=4` 即可）、无越界歧义；后者要处理
  `minStorageBufferOffsetAlignment`/`buffer_reference_align` 与越界 UB。这条同时满足"整数纪律"。
- 帧槽：**高度缓冲静态**（无 per-frame 风险）；tile 表 / 命令数组 / count **按帧槽多份**。

### 2.4 同步与帧流

单帧命令顺序（`frames in flight = 2~3`，每槽独立 fence/semaphore）：

```
BeginCommandBuffer
  vkCmdFillBuffer(count 段, 0)                     ← 必须每帧归零
  dispatch(compute: 剔除 + 写 tile 表 + 写命令数组 + 原子加 count)
  vkCmdPipelineBarrier2(COMPUTE_SHADER/SHADER_WRITE
                        → DRAW_INDIRECT|MESH_SHADER_EXT/INDIRECT_COMMAND_READ|SHADER_READ)
BeginRendering(color+depth)
  bind mesh pipeline / bind 描述符 / push constant / vkCmdSetViewport/Scissor
  vkCmdDrawMeshTasksIndirectCountEXT(cmd, cmdBuf, cmdOff, countBuf, cntOff, maxDraws, sizeof(VkDrawMeshTasksIndirectCommandEXT))
EndRendering
EndCommandBuffer
Submit(wait = imageAvailable + inFlight fence) / Present(wait = renderFinished)
```

要点：
- mesh 阶段的管线屏障位是 `VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT`（`TASK_SHADER` 位同理）。
- count 段归零（`vkCmdFillBuffer`）与上一帧对同一段的消费必须**不同帧槽**，否则竞态。
- 深度：`VK_FORMAT_D32_SFLOAT`，**reversed-Z**（clear=0.0，`compareOp=GREATER_OR_EQUAL`，
  投影用 `-1..0` 的深度范围），可与 infinite far 组合——远处地形深度精度更好。
- 开发期一律开 `VK_LAYER_KHRONOS_validation`，并用 sync validation 检查 hazard。

### 2.5 本机可读参考实现（建议以此起步，别从空白写）

| 路径 | 用途 |
|---|---|
| `D:\SaschaWillems\Vulkan\examples\meshshader` | mesh 管线创建/绘制骨架 |
| `...\indirectdraw` | `vkCmdDrawIndirect` 家族、多命令提交 |
| `...\computecullandlod` | compute 剔除 + LOD + 间接绘制（V3/V4 直接对照） |
| `...\bufferdeviceaddress` | BDA 用法与 GLSL `buffer_reference` |
| `...\terraintessellation` | 另一条地形路线（tessellation）对照 |

---

## 3. 目标架构

### 3.1 数据流

```
高度页缓冲（BDA uint32[]，页对齐）
页表（每页：缓冲元素偏移 / texel 尺寸 / 驻留标志 / 高度界）
tile 表（每 tile：texel 原点(整数) / cells / lod / 高度界 / flags）  ← "画在哪、读哪"的唯一权威
        │        V1-V3：CPU 合成；V4：compute 生成
        ▼
命令数组 VkDrawMeshTasksIndirectCommandEXT[] + count（V4）
        ▼
单 mesh 管线「TerrainGrid」：1 线程 = 1 格 = 4 顶点 + 2 图元
  texel  = tile.texel_origin + (cell << lod)      ← 全整数
  h      = heights[texel.y * stride + texel.x]     ← 全整数索引
  world  = vec3(grid_x, grid_y, h) * (texel_world_size<<lod, ..., height_scale)
  clip   = vp * vec4(world, 1)
```

**核心不变式**：tile 记录是"读什么数据、画在哪"的唯一来源；采样位置与世界坐标由它派生。
源工程里分散的三处信息（模型平移、`uTexLayer`、`uSize`）被收敛为一条记录。

### 3.2 结构定义（C++ 与 GLSL 两份，字段逐一对齐）

```cpp
// C++（std430 对齐；补到 16B 整倍数）
struct TerrainTile                   // 32B
{
    uint32_t texel_origin_x;         // 全局 texel 原点（整数，精确）
    uint32_t texel_origin_y;
    uint32_t cells;                  // 格数 = grid_dim - 1
    uint32_t lod;                    // 网格步长 = 1 << lod（texel）
    float    height_min;             // 高度界：剔除 + 裂缝/裙深估算
    float    height_max;
    uint32_t page_index;
    uint32_t flags;
};
static_assert(sizeof(TerrainTile) == 32);
```

```glsl
// GLSL（与上面逐字段一致）
struct TerrainTile
{
    uint  texel_origin_x;
    uint  texel_origin_y;
    uint  cells;
    uint  lod;
    float height_min;
    float height_max;
    uint  page_index;
    uint  flags;
};
```

对齐规则：`layout(scalar)` 下无隐式填充，`uint` 4B / `float` 4B 连续即可（附录 A 已验证）。
若不用 `scalar` 而用 `std430`，`uint` 序列仍连续，但结构体数组的 stride 会被补到 16B——
本项目本来就按 32B 设计，两者一致。

### 3.3 裂缝的本质与消除手段（**LOD 接缝的正确结论**）

**T 型交点缝是固有几何问题，不能靠采样方式消除。**

- 相邻 LOD 共享一条边时：细侧是折线（穿过多个顶点），粗侧是弦（只连两端点）。
  两者只有在该边上的高度函数为**线性**（平坦或恒定坡度）时才重合。
- "2 的幂步长 + 同一高度函数"能保证的是**顶点级重合**：粗格点位置是细格点子集 →
  取到同一个 texel → 同一个高度值。这**不等于**边内部重合。

三种工程手段：

| 手段 | 说明 | 代价 | 本方案 |
|---|---|---|---|
| **a. 裙边（skirt）** | 每个 tile 边缘额外向下挤出一圈三角形，遮住缝隙 | 少量额外三角形；需确定深度 | **V3 默认** |
| b. 顶点形变（geomorph / CDLOD morphing） | 细侧过渡带顶点向粗侧弦插值，视觉平滑、无额外几何 | 需要 morph 因子与过渡带判定 | V3+ 可选优化 |
| c. 过渡带缝合 | 粗 tile 外圈改用细步长（与邻居一致） | tile 需知邻居 LOD，网格不再均匀 | 不采用 |

**双线性采样的真实作用**：把"高度函数"与"网格分辨率"解耦（网格点落在 texel 之间时避免阶梯），
并让不同 LOD 在**任意**位置取到同一函数值——这使裂缝**有界且可量化**（是 a/b 的前提）。
在"2 的幂 + texel 对齐"时它退化为直接取纹素（fraction = 0），因此**它不是消裂手段，也不该被当成消裂手段**。

**裙深怎么定（可量化）**：裂缝深度 = 共享边上 `|粗表面高度 - 细表面高度|` 的最大值，
上界由该处高度曲线的曲率决定。实践两种取法：
1. 保守：`skirt_depth = k * (height_max - height_min)`（k ≈ 0.5~1，本 tile 的极差已知，零额外数据）；
2. 精确：由父级 tile 的误差度量给出（更紧，但需要层级数据）。
→ 判据：`skirt_depth ≥ 实测最大裂缝深度`（见 §5 的自动量化判据）。

**对角线约定**：格内固定按 `(TL,BL,TR)+(TR,BL,BR)` 或 `(TL,TR,BR)+(TL,BR,BL)` 之一对角化，
全局一致。不一致只会造成亚 texel 级差异，但保持一致成本为零。

### 3.4 数据注入：三种通路与选择

| 通路 | 适用 | 本方案 |
|---|---|---|
| **push constant** | 每 draw 少量标量（tile 索引、stride、缩放、vp 矩阵） | ✅ 主用（本机 256B，占用 104B） |
| **BDA（`buffer_reference`）** | 大数组（高度、tile 表） | ✅ 主用（V4 compute 写入后地址不变） |
| UBO / SSBO 描述符 | 小表、需要 `vkCmdUpdateBuffer` 或动态偏移时 | 备选（无需 `shaderInt64`，移植性更好） |

**最小可用路线**：V1 也可以完全不用 BDA——把高度缓冲作为 `binding=0` 的 SSBO 描述符、
相机 UBO 作为 `binding=1`，一切走描述符。BDA 的价值在 V4（compute 写入的表、地址稳定、少一次绑定）。
建议 V1 先用描述符版打通，V2 再切 BDA（两份 shader 仅声明头不同）。

### 3.5 整数纪律（Vulkan 版）

```
cell_id = gl_WorkGroupID.x * 64 + gl_LocalInvocationIndex   // uint
gx      = cell_id % cells_per_row
gy      = cell_id / cells_per_row
texel_x = tile.texel_origin_x + (gx << tile.lod)            // uint
index   = texel_y * stride + texel_x                        // uint ← 到此处全整数
h       = float(heights[index])                             // 最后才转 float
world   = vec3(float(gx), float(gy), h) * scale             // 最后一步
addr    = heights_base + index * 4                          // 唯一 64 位出口（VkDeviceAddress）
```

- 小整数（≤2²⁴）在 float 中精确 → 网格局部坐标与 texel 索引可以放心用 float；
- **世界坐标**与"uv→世界"的来回乘除是误差来源 → 世界坐标只在 CPU 侧以 double 累加，
  shader 里只出现 tile 局部小坐标（`shaderFloat64=false`，本机连兜底都没有）；
- 大世界场景需要相机相对渲染（把视点平移量记在 CPU double 里，shader 用相对坐标 + 低精度绝对坐标
  供雾/远景使用）——独立工程可以从一开始就做对。

---

## 4. 分阶段实施

每阶段以「改 → 编译（`glslc` + `spirv-val`）→ 运行（validation 零 error）→ 截图/自检 → tag」收尾。

### V1 — 单 tile 出图（打通全链）

**内容**：mesh 管线 + 高度 SSBO + 相机 + 深度；单 tile、固定 `cells`。

建立顺序（每步都可独立验证）：
1. 交换链/深度/帧槽骨架（照 `meshshader` 示例）；
2. `glslc` 编译附录 A 骨架，`spirv-val` 过；管线创建成功
   （`pVertexInputState = nullptr`、`pInputAssemblyState = nullptr`、`polygonMode = FILL`）；
3. CPU 生成正弦环高度写入缓冲（staging → `vkCmdCopyBuffer`）；
4. `vkCmdDrawMeshTasksEXT(cmd, groups, 1, 1)`，`groups = ceil(cells/8)²`；
5. 深度 reversed-Z 打开。

**判据**：出图；网格无空洞；validation 零 error（含 sync validation）；
抽样 10 个 (gx,gy)，shader 输出高度与 CPU 同索引值一致。

### V2 — 多 tile + 剔除

**内容**：CPU 合成 tile 表（`BuildTileList(view, lod_policy)` **纯函数**，V4 原样搬进 compute）；
tile 级视锥剔除（用 `height_min/max` 作 Z 界）；
命令数组 + `vkCmdDrawMeshTasksIndirectCountEXT`（count 由 CPU 写或先直接提交多次 draw）。

**要点**：tile 表/命令数组按帧槽多份；`VkDrawMeshTasksIndirectCommandEXT{groupCountX, groupCountY=1, groupCountZ=1}`。
宽 tile 如果超出 `maxMeshWorkGroupTotalCount`（本机 4194304）要拆——1K² 格不会碰到。

**判据**：8×8 tile 拼合无重复/空洞（边界高亮材质）；视野外 tile 的 `groupCountX == 0`。

### V3 — LOD 与裂缝处理

**内容**：
- `BuildTileList` 引入 LOD（相机距离/屏幕空间误差 + `height_min/max` 极差）；
- **裙边**：tile 边缘额外 emit 一圈三角形（照 §3.3 的两种裙深取法）；
- 构建 per-tile min/max 金字塔（四叉树）供 LOD 误差度量与地平线遮挡；
- 「2 的幂步长」纪律 + 对角线约定统一。

**判据（两条，均可自动判定）**：
1. **顶点级重合**：共享边上所有粗格点位置，用细网格插值出的高度与粗网格顶点高度差 == 0；
2. **裂缝量化**：沿共享边密集采样 `|粗表面高度 - 细表面高度|` 取最大值 → 断言
   `skirt_depth ≥ 该最大值`。这条把"看起来没缝"变成可测的数字。

### V4 — GPU-Driven

**内容**：compute 写 tile 表 + 命令数组 + count（原子加）；`vkCmdFillBuffer` 归零 count；
`vkCmdDrawMeshTasksIndirectCountEXT` 一次提交全部 tile；每帧槽独立缓冲。

**三条硬约束**：
1. count 归零 → append → 消费 的批次顺序正确，且归零目标与上一帧消费**不同帧槽**；
2. tile 表/命令数组的 usage 必须含 `INDIRECT_BUFFER`（+ `SHADER_DEVICE_ADDRESS`）；
3. 屏障用 `VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT` / `DRAW_INDIRECT`（sync2 写法更清晰）。

**判据**：与 V3 **逐像素一致**（同相机同 LOD）；绘制调用数降为 1；
`vkCmdDrawMeshTasksIndirectCountEXT` 的 `maxDrawCount` 与 count 实测值打印核对。

### V5 — 可选实验（独立工程的独有价值）

| 实验 | 方法 | 价值 |
|---|---|---|
| 纹理路线 A/B | 高度改 `R16_UNORM` 纹理 + sampler（给 binding 加 `VK_SHADER_STAGE_MESH_BIT_EXT`），硬件双线性 vs 手动双线性 | 量化采样成本与代码复杂度（ULRE 里做不了） |
| 稀疏驻留 | `VK_IMAGE_CREATE_SPARSE_BINDING_BIT` + HEAP 内存 + `vkQueueBindSparse` + 页/居民化 | 本机 `sparseResidency*` 全 true；验证"巨大虚拟高度图"路线 |
| 网格着色器 LOD 变体 | 每 tile 网格分辨率随距离变化（非幂等步长） | 需回到双线性采样 + 定量评估 |
| Task Shader 剔除 | 把 tile 剔除从 compute 搬到 task shader（`maxTaskPayloadSize` 本机 65504） | 对照两种剔除位置 |

---

## 5. 验证矩阵

| 项 | 手段 | 判据 |
|---|---|---|
| GLSL 编译/校验 | `glslc --target-env=vulkan1.3` + `spirv-val` | 零 error（附录 A 已通过） |
| 管线创建 | mesh 管线（vi/ia = NULL、polygonMode = FILL） | `VK_SUCCESS`；validation 无 VUID |
| 运行时正确性 | `VK_LAYER_KHRONOS_validation`（含 sync validation） | 零 error/零 hazard 报告 |
| mesh 输出正确性 | 抽样 10 个 (gx,gy) 输出高度做色 | 与 CPU 同索引值一致 |
| 边缘组（非 2 的幂 cells，如 127/100） | 截图 + validation | 无空洞、无越界写 |
| tile 拼接 | 边界高亮材质 | 无重复/空洞 |
| LOD 顶点重合 | compute/CPU 自检 | 共享边粗格点高度差 == 0 |
| **裂缝量化** | 边界密集采样取 max 差 | `skirt_depth ≥ max` |
| 剔除正确性 | 命令数组 dump | 视野外 tile `groupCountX == 0` |
| 性能 | timestamp query（period 52.08 ns） | 记录 V1→V4 的 mesh 阶段耗时/三角数/命令数 |
| 回归 | 同相机同位置截图 | V2/V3/V4 与前一阶段在非 LOD 变化区域逐像素一致 |

---

## 6. 风险与开放问题

| # | 风险 | 处理 |
|---|---|---|
| R1 | **mesh 输出是四重预算**（vertices / primitives / memory / components），只盯 `maxMeshOutputVertices` 会在别处失败 | 按规范下限设计（256 顶点/256 图元），管线创建即失败便于早发现；本机实测见 §2.2 |
| R2 | 用户 varying 未写 `layout(location)` → `'location' : SPIR-V requires location for user input/output` | 实测踩过：mesh 阶段的 `out` 数组成员必须显式 location |
| R3 | 顶点输出写入方式 | 用户 varying 用 `varying[i] = ...` 直接数组索引（`gl_MeshVerticesEXT[]` 只含 `gl_Position` 等内建）；已编译验证 |
| R4 | BDA 依赖 `bufferDeviceAddress + scalarBlockLayout + shaderInt64` | 三项作为硬性设备门槛在启动时断言；不满足则退描述符版 |
| R5 | `shaderFloat64 = false`（本机） | 世界坐标精度只能靠 CPU double + 相机相对；不要指望 shader 内 double |
| R6 | 帧槽竞态（本帧 compute 写 / 上一帧还在读） | 从 V2 起就按帧槽多份化，别等到 V4 |
| R7 | 裂缝随 LOD 级差增大 | 裙深用可量化判据（§5）；先支持相邻级 ±1，跳级在 V3 自检里覆盖 |
| R8 | 上 sparse 的诱惑 | 硬件支持但工程量独立（HEAP/page size/mip tail/绑定队列）；列为 V5 实验，不进主线 |
| R9 | 平台差异（NV 的 256 顶点上限、不同 `maxPushConstantsSize`） | 全部上限从 `vkGetPhysicalDeviceProperties2` 查询后决定分组，不写死 |
| R10 | 高度缓冲与 tile 表混在一张大 buffer 里导致对齐全乱 | 一缓冲一用途；高度只做"基地址 + 整数索引"（§2.3 寻址规则） |

---

## 7. 里程碑与回滚

- 分支 `terrain-experiment`，每阶段 tag：`v1-single-tile` / `v2-tiles` / `v3-lod` / `v4-gpudriven`。
- 每个阶段保持"可运行且截图可对比"——回归判据依赖同相机同位置截图，因此每阶段都保留一个
  `--screenshot <phase>` 模式（固定相机路径），避免"改坏了才发现"。
- GLSL 与 C++ 侧的 tile 结构若发生字段变化：两侧同一次提交内改完（附 `static_assert`），
  不允许"先改一侧跑起来"。

---

## 附录 A：GLSL 骨架（本机 glslc 1.4.357 + spirv-val **已编译通过**）

经验要点（编译过程实测）：
- 用户 varying 必须显式 `layout(location = N) out <type> name[];`，否则报
  `'location' : SPIR-V requires location for user input/output`；
- 用户 varying 通过**直接数组索引写入**（`vUV[i] = ...`）；
- 需要的扩展：`GL_EXT_mesh_shader` / `GL_EXT_buffer_reference` / `GL_EXT_scalar_block_layout` /
  `GL_ARB_gpu_shader_int64` / `GL_EXT_shader_explicit_arithmetic_types_int64`；
- 生成的 SPIR-V capability：`Int64` / `MeshShadingEXT` / `PhysicalStorageBufferAddresses`。

```glsl
// ---------- terrain.mesh ----------
#version 460
#extension GL_EXT_mesh_shader : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(local_size_x = 64) in;                                   // 64 线程 = 8×8 格
layout(triangles, max_vertices = 256, max_primitives = 128) out;

struct TerrainTile
{
    uint  texel_origin_x;
    uint  texel_origin_y;
    uint  cells;                                                // 格数 = grid_dim - 1
    uint  lod;
    float height_min;
    float height_max;
    uint  page_index;
    uint  flags;
};

layout(push_constant) uniform PC                                 // 104B < 256B（本机实测上限）
{
    mat4     vp;
    uint64_t addr_heights;
    uint64_t addr_tiles;
    uint     tile_index;
    uint     heights_stride;                                    // texel/行
    float    texel_world_size;                                  // 一个 texel 的世界尺寸
    float    height_scale;                                      // 高度单位 -> 世界单位
    float    skirt_depth;
    uint     _pad;
} pc;

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer HeightRef { uint data[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer TileRef   { TerrainTile t[]; };

#define HBUF HeightRef(pc.addr_heights)
#define TBUF TileRef(pc.addr_tiles)

layout(location = 0) out vec2 vUV[];                             // 必须显式 location

float SampleHeight(uint tx, uint ty)                             // 整数索引；16 位值在低 16 位
{
    return float(HBUF.data[ty * pc.heights_stride + tx]);
}

void main()
{
    const TerrainTile tile = TBUF.t[pc.tile_index];
    const uint step    = 1u << tile.lod;                         // 2 的幂步长
    const uint per_row = (tile.cells + 7u) / 8u;
    const uint gid     = gl_WorkGroupID.x;
    const uint gx0     = (gid % per_row) * 8u;
    const uint gy0     = (gid / per_row) * 8u;

    // ⚠ SetMeshOutputsEXT 必须全组一致：边缘组用 2D 裁剪算出有效格数
    const uint cols  = (gx0 >= tile.cells) ? 0u : min(8u, tile.cells - gx0);
    const uint rows  = (gy0 >= tile.cells) ? 0u : min(8u, tile.cells - gy0);
    const uint valid = cols * rows;

    SetMeshOutputsEXT(valid * 4u, valid * 2u);
    if (valid == 0u)
        return;

    const uint lx = gl_LocalInvocationIndex & 7u;
    const uint ly = gl_LocalInvocationIndex >> 3u;
    if (lx >= cols || ly >= rows)
        return;

    const uint cx = gx0 + lx;
    const uint cy = gy0 + ly;
    const uint tx = tile.texel_origin_x + (cx << tile.lod);      // 整数域
    const uint ty = tile.texel_origin_y + (cy << tile.lod);

    const uint compact  = ly * cols + lx;                        // 紧凑槽位（勿用 gl_LocalInvocationIndex）
    const uint base_vid = compact * 4u;

    const float h0 = SampleHeight(tx,         ty        );
    const float h1 = SampleHeight(tx + step,  ty        );
    const float h2 = SampleHeight(tx,         ty + step );
    const float h3 = SampleHeight(tx + step,  ty + step );

    const float gx[4] = float[4](float(cx), float(cx + 1u), float(cx),       float(cx + 1u));
    const float gy[4] = float[4](float(cy), float(cy),       float(cy + 1u), float(cy + 1u));
    const float gh[4] = float[4](h0, h1, h2, h3);

    const float cs = pc.texel_world_size * float(step);

    for (uint i = 0u; i < 4u; ++i)
    {
        const vec3 world = vec3(gx[i] * cs, gy[i] * cs, gh[i] * pc.height_scale);
        gl_MeshVerticesEXT[base_vid + i].gl_Position = pc.vp * vec4(world, 1.0);
        vUV[base_vid + i] = vec2(gx[i], gy[i]);
    }

    // 对角线约定固定：(TL,BL,TR) + (TR,BL,BR)
    gl_PrimitiveTriangleIndicesEXT[compact * 2u + 0u] = uvec3(base_vid + 0u, base_vid + 2u, base_vid + 1u);
    gl_PrimitiveTriangleIndicesEXT[compact * 2u + 1u] = uvec3(base_vid + 1u, base_vid + 2u, base_vid + 3u);
}
```

```glsl
// ---------- terrain.frag（占位：导数法线 + 简单光照，V1 用）----------
#version 460
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
void main()
{
    outColor = vec4(normalize(vec3(vUV, 0.7)) * 0.5 + 0.5, 1.0);
}
```

编译命令（本机已跑通）：

```bash
"$VULKAN_SDK/Bin/glslc.exe" --target-env=vulkan1.3 -o terrain.mesh.spv terrain.mesh
"$VULKAN_SDK/Bin/glslc.exe" --target-env=vulkan1.3 -o terrain.frag.spv terrain.frag
"$VULKAN_SDK/Bin/spirv-val.exe" --target-env vulkan1.3 terrain.mesh.spv
```

**V3 裙边片段**（接在 `main()` 的顶点写入之后，示意）：

```glsl
// 每个边缘格额外向 -Z 挤出一圈三角形遮裂：以格为单位判断是否贴 tile 边缘，
// 贴边时把该格的边缘顶点复制一份，高度改为 h - pc.skirt_depth。
// 裙深取法见 §3.3；占用额外的 max_vertices/max_primitives 预算，需同步调整 layout 常量。
```

### C++ 侧 push constant 对应结构（104B）

```cpp
struct TerrainPushConstants            // std430/标量布局，无需额外填充
{
    glm::mat4     vp;                  // 64
    uint64_t      addr_heights;        // 8
    uint64_t      addr_tiles;          // 8
    uint32_t      tile_index;          // 4
    uint32_t      heights_stride;      // 4
    float         texel_world_size;    // 4
    float         height_scale;        // 4
    float         skirt_depth;         // 4
    uint32_t      _pad;                // 4
};
static_assert(sizeof(TerrainPushConstants) == 104);
```

---

## 附录 B：自检清单（启动时一次、开发全程）

```cpp
// 1) 设备门槛断言（不满足直接退出，别等到黑屏）
VkPhysicalDeviceMeshShaderFeaturesEXT mesh_f{...};      // meshShader 必查
VkPhysicalDeviceVulkan12Features     vk12{...};         // bufferDeviceAddress / scalarBlockLayout / drawIndirectCount
VkPhysicalDeviceFeatures             f10{...};          // shaderInt64
// 2) 上限查询后决定分组（不要写死 1024/256）
VkPhysicalDeviceMeshShaderPropertiesEXT mp{...};
// 3) 管线创建自检：vi/ia = NULL、polygonMode = FILL、stageFlags 与 descriptor set 的 stage 一致
// 4) 开发期：VK_LAYER_KHRONOS_validation + sync validation，日志零 error
```

```bash
# 设备数据自查（与本文表格对照）
"C:/VulkanSDK/1.4.357.0/Bin/vulkaninfoSDK.exe" > vk.txt
grep -E "maxMesh|meshShader|bufferDeviceAddress|shaderInt64|maxPushConstants|sparseResidency" vk.txt
```

## 附录 C：术语

| 术语 | 含义 |
|---|---|
| 页（page） | 高度数据的流式单位，固定 texel 尺寸，落在高度缓冲的一段整数元素偏移上 |
| tile | 绘制单位：`cells × cells` 个格 + lod；"画在哪、读哪"的唯一权威 |
| 格（cell） | tile 内一个四边形，对应 mesh 阶段一个线程（4 顶点 + 2 图元） |
| 组（workgroup） | 64 线程，覆盖 8×8 格 |
| 裂缝 / T 型缝 | 相邻 LOD 共享边上，细侧折线与粗侧弦的几何偏差 |
| 裙边（skirt） | 边缘额外下压的一圈三角形，用于遮盖裂缝 |
| 高度界金字塔 | 每层 tile 的高度 min/max 树，用于剔除与 LOD 误差度量（**不是** mipmap） |
| BDA | `VkDeviceAddress` / GLSL `buffer_reference`，shader 直接用 64 位地址寻址缓冲 |
