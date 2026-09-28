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
> **2026-09-28 指示（覆盖本文档首版口径）**：**不走 Task Shader**，走 **ComputeShader + MeshShader**；
> 先由 **CPU 生成 Indirect Command Buffer**、**暂不做剔除**，将来迁进 compute 并在其中做 Frustum 剔除；
> **开发期以稳定性为准、不追性能**；相位梯拆为「多 tile → LOD → 外扩遮缝 → ICB 化 → GPU-driven → 剔除」（§5）；
> **遮缝方式由「向下挤出裙边」改为「与邻居外扩重叠」**（§3.3）—— ε 只需避开共面，故删掉裙深标定。
>

---

## 0. 决策速览

| 议题 | 决策 | 依据 |
|---|---|---|
| 网格生成 | MeshShader 内由索引生成 XY，Z 取高度场 | 无 VBO/EBO，CPU 只提交 tile 记录 |
| 顶点缓冲 | 完全不存在（mesh 管线无顶点输入阶段） | `pVertexInputState`/`pInputAssemblyState` 必须为 NULL |
| 高度存储 | **storage buffer + BDA**（`uint32` 装 16 位值起步） | 整数索引寻址；无采样器状态；见 §2.3 |
| 分块 | 两级：**页**（流式单位）+ **tile**（绘制单位） | 避免 tile 跨页两级间接 |
| Texture2DArray | 不用 | 层数（规范下限 256 / 本机 2048）与整数组共享 mip 链；**本工程可读 mesh 阶段纹理，故理由只在"无 mip 需求 + 按层流式"** |
| 纹理路线 | 独立工程里**可行**（可给 binding 加 `VK_SHADER_STAGE_MESH_BIT_EXT`），列为 A/B 对照实验 | ULRE 版受限，本工程无此约束 |
| Sparse 稀疏驻留 | 硬件支持（本机 `sparseResidency*` 全 true），但**不作为第一阶段** | 工程量（HEAP 内存/page size/mip tail/`vkQueueBindSparse`）另计 |
| Mipmap | 高度图**不做 mip**；做 per-tile min/max 金字塔 | mip 改变几何函数；远景开销在顶点数不在采样 |
| 裂缝（LOD 接缝） | **外扩重叠（与邻居重叠 1 格）**为默认；形变为可选优化 | 见 §3.3（T 型缝无法靠采样消除；**不做向下裙边**） |
| 参数传递 | 整数域贯穿到算出**元素索引**为止 | 浮点仅用于最终坐标/高度；`VkDeviceAddress` 是唯一 64 位出口 |
| 世界坐标精度 | CPU double + tile 局部小浮点（`shaderFloat64` 本机 = false） | shader 内不能靠 double 兜 |
| 绘制路径 | 单管线 + `vkCmdDrawMeshTasksIndirectCountEXT` | V4 一次提交全部 tile |
| 帧资源 | 一开始就按帧槽多份化（tile 表/命令/count） | 独立工程可直接做对 |

---

## 0.1 设计基线（2026-09-28）：采纳 `doc/Terrain Vulkan 1.4.rtf` 的决策

本工程是独立 Vulkan 工程，不受 ULRE 约束，但**设计意图**以工作区里更新的
`doc/Terrain Vulkan 1.4.rtf`（Task+Mesh / GPU-Driven / BDA）为准。与本文档首版的差异如下
（**冲突处以本表为准**）：

| 议题 | 首版（本文档 v1） | 现口径（采纳 RTF） | 影响 |
|---|---|---|---|
| GPU-Driven 机制 | V4 用 compute 生成 tile 表 + 间接命令 + count | **ComputeShader + MeshShader 两阶段**（**不用 Task Shader**）：先 CPU 生成 Indirect Command Buffer（V4），再迁进 compute（V5）；剔除放最后（V6） | §5 改为 V4 ICB → V5 GPU-driven → V6 剔除；不启用 `taskShader` |
| 网格拓扑 | 1 线程 = 1 格 = 4 顶点（8×8 格 = 256 顶点） | **9×9 = 81 顶点对应 8×8 格**（顶点复用）；外扩重叠后最坏 10×10 ⇒ **100 顶点 / 162 图元** | 顶点带宽降约 68%；索引发射仍是每线程 2 个图元 |
| 高度存储 | `uint32[]` 装 16 位值 | **原生 `uint16_t[]`**（`storageBuffer16BitAccess` + `GL_EXT_shader_16bit_storage`） | 内存 / 带宽减半；本机实测 `storageBuffer16BitAccess / shaderInt16 = true` |
| 地表法线 | 片元 `dFdx/dFdy` | **mesh 阶段中心差分**（`normalize(vec3(h_l-h_r, h_d-h_u, 2*world_step/height_scale))`） | 低模无刻面感；少一次法线贴图 |
| 相机传递 | push constant 里塞 `mat4 vp`（PC 104B） | **精简 PC（两地址 + tile_index + stride + 两个尺度 + `overlap_epsilon` + world_texels = 104B）**，相对视口矩阵另路下发 | 独立工程可用 UBO 或 BDA 传矩阵 |
| 遮缝方式 | V3 默认启用向下裙边 | **改为「与邻居外扩重叠」**：网格格域 = `cells + 1`，外扩带高度 −ε | 与 §3.3 一致（T 型缝必须遮）；**不需要裙深标定** |
| 大世界精度 | CPU double + tile 局部小坐标 | **相机相对渲染**（relative_vp） | 与 §3.5 一致，另需把相对视口矩阵下发到 shader |

**预算核对**（对本机 1024 上限与规范 256 下限同时成立）：顶点 ≤**100** ≤ 256、图元 ≤**162** ≤ 256
⇒ **NV 系 `maxMeshOutputVertices=256` 的设备同样能跑**（本文档原「256 顶点刚好卡下限」的算法在 81/100 拓扑下更宽松）。

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

需要开启的结构：`VkPhysicalDeviceMeshShaderFeaturesEXT{meshShader=TRUE, taskShader=TRUE}`（Task 路径，见 §0.1）、
`VkPhysicalDeviceVulkan12Features{bufferDeviceAddress, scalarBlockLayout, drawIndirectCount}`、
`VkPhysicalDeviceFeatures{shaderInt64}`。
若用 16 位存储（省内存）另需 `storageBuffer16BitAccess` + `GL_EXT_shader_16bit_storage`。

### 2.2 设备上限（Intel Arc 140T / 驱动 101.8991）

| 上限 | 本机实测 | 规范保证下限 | 本方案占用 |
|---|---|---|---|
| `maxMeshOutputVertices` | **1024** | 256 | **100**（外扩后最坏：角组 10×10；内部组 81） |
| `maxMeshOutputPrimitives` | **1024** | 256 | **162**（角组 9×9×2；内部组 128） |
| `maxMeshOutputMemorySize` | 524288 B | 32768 B | 远低于 |
| `maxMeshOutputComponents` | 128 | — | ≤ 16（位置 + UV） |
| `maxMeshWorkGroupInvocations` | 1024 | 128 | 64 |
| `maxMeshWorkGroupSize` | (1024,1024,1024) | (128,1,1) | (64,1,1) |
| `maxTaskPayloadSize` | 65504 | — | 主线不用 Task（§0.1）；仅 V7 对照实验时涉及 |
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

### 2.6 对齐与尺寸约束（**走 buffer 不是纹理；2 的幂加在 LOD 上，不在数据尺寸上**）

**结论先行**：mesh 顶点数不是瓶颈（≤100 顶点 / ≤162 图元，对上限 1024 与规范下限 256 都有一倍以上余量）；
"超大高度图要对齐"是对的，但**纹理那套对齐规则一条都不适用**，真正需要 2 的幂的是 **LOD 的格步长与 tile 跨度**，
不是高度数据（或纹理）的尺寸。

**为什么高度走 buffer 而不是纹理（本工程有得选，但主线仍走 buffer）**

1. **ULRE 版是被 E7 卡死的**：那边 bindless 纹理 binding 的 `stageFlags` 只有 `FRAGMENT|COMPUTE`，
   mesh 阶段采样不到纹理。**本工程没有这条约束**（可给 binding 加 `VK_SHADER_STAGE_MESH_BIT_EXT`），
   所以把「纹理路线 A/B」留在 V7 做对照实验——主线的理由见下面 2–4，与那条约束无关。
2. **整数域**：`R16_UINT` **不能硬件双线性**、不能线性 blit；要硬件插值就得存 UNORM
   （归一化丢高度语义、还要量化还原）。走 buffer 时顶点落在 texel 上（fraction = 0），
   手动 4 读 + lerp 实际退化为单次读，滤波成本可以忽略。
3. **少一整套状态**：无 sampler（filter / address mode）、无 image layout 转换（纹理要在 compute 写/读还得加 barrier）、
   越界读就是普通索引加减（中心差分读 ±step 天然合法，§3.5）、分块不受 `maxImageArrayLayers` 限制。
4. **地址稳定**：BDA 地址在 V5（compute 写表）后不变，compute 与 mesh 用同一地址读，无需重绑。

代价（别只看好处）：没有硬件滤波 / 各向异性 / 自动归一化（高度由 shader 乘 `height_scale`），
且 stride 与对齐要自己管（本节 (1)）。**与"不做 mipmap"是两件事**：mip 是另一个高度函数、
破坏粗/细同值前提，和走 buffer 还是纹理无关。


**（1）buffer 侧只有三条约束，全都与尺寸是否 2 的幂无关**

| # | 约束 | 出处 | 对本方案的含义 |
|---|---|---|---|
| A1 | 物理指针访问必须带 `Aligned` 操作数，其值须是**被指向类型中最大标量宽度**的倍数 | `VUID-StandaloneSpirv-PhysicalStorageBuffer64-06314` | `uint16_t[]` ⇒ Aligned **2**；tile 行结构最大标量 4B ⇒ Aligned **4**（均已在 SPIR-V 里实测到） |
| A2 | 指针值必须 ≥ 该 `Aligned` | `VUID-RuntimeSpirv-PhysicalStorageBuffer64-06315` | 高度缓冲基址 ≥2B、tile 表基址 ≥4B 对齐；取址后**断言**（驱动实测给 ≥16B、通常 256B，但不要假设） |
| A3 | 被引用缓冲必须带 `SHADER_DEVICE_ADDRESS` | `VUID-RuntimeSpirv-PhysicalStorageBuffer64-11819` | 已在 §3.4/§0.1 的通路表里 |

纹理路线的那些规则（`VkImage` texel block、`vkCmdCopyBufferToImage` 的 `bufferRowLength`/`bufferImageHeight`、
`optimalBufferCopyRowPitchAlignment`、mip 链、array layer、sampler 寻址模式）**一条都不用管**。

**（2）2 的幂的正确归属**

| 对象 | 要不要 2 的幂 | 原因 |
|---|---|---|
| 格步长 `1 << lod` | **要** | 粗格点必须落在细格点上；且 texel = 位移而非乘除法 |
| tile 跨度 `kTileSpan` | **要**（取 2 的幂最省心） | 必须是最大格步长的整数倍 ⇒ 任意 LOD 下 tile 原点都是所有步长的共同倍数（顶点级重合的前提） |
| 世界高度图尺寸 | **不要** | 只需能被 `kTileSpan` 整除（配置校验拒绝不整除）；例 8192×6144 = 32×24 tile 合法。
若不整除：**填充到整数倍**是可接受的替代——多分配几行几列、按 clamp 复制填边（世界随之略大于资产，程序化地形无所谓） |
| 行 stride（texel） | **不要**（但建议 4 的倍数） | 只是我们自己的 `uint32` 步长；4 的倍数利于向量化读 |
| 纹理（若走 R16_UNORM 路线） | **不要** | Vulkan **没有** POT 纹理要求（那是 GL 1.x / ES 1.x 的遗留约束）；那时要管的是 texel block 与拷贝行距 |

**（3）超大高度图真正的天花板（与对齐无关，全是容量）**

| 上限 | 本机实测 | 含义 |
|---|---|---|
| `maxStorageBufferRange` | 4 294 967 292（≈4 GiB） | 单缓冲可寻址范围 |
| `maxMemoryAllocationSize` | 4 294 901 760（≈4 GiB − 64 KiB） | **单次分配**上限（二者取小） |
| 16 位元素数 | ≈2³¹ texel | 世界边长 ≤ **46340** texel（1 m/texel ⇒ 46 km 见方） |

超出这套天花板的出路：**多 buffer + 地址数组**（"页"机制，`page_index` 字段已预留）或 **sparse 驻留**（V7 实验）。
本方案默认量级（256 texel/tile、1 m/texel、8192 m 世界）= 8192² × 2 B = **128 MB**，离天花板三个数量级。

**（4）16 位路径的两个实测细节（本机 glslc 1.4.357 / spirv-val 通过）**

```glsl
#extension GL_EXT_shader_16bit_storage : require
layout(buffer_reference, scalar, buffer_reference_align = 2) readonly buffer HeightRef { uint16_t data[]; };
...
return float(uint(HBUF.data[idx]));          // ✅ 必须两级转换
// return float(HBUF.data[idx]);             // ✗ glslc: 'constructor' : can't convert
```

- SPIR-V 多出 `OpCapability StorageBuffer16BitAccess`（实测：32 位版 9536 B → 16 位版 9608 B）；
- `OpLoad %ushort … Aligned 2`、`OpLoad %TerrainTile … Aligned 4`、`ArrayStride 2`（高度）/`ArrayStride 32`（tile 行）；
- **可移植性门**：启动断言 `storageBuffer16BitAccess`（本机 true）——
  `VUID-RuntimeSpirv-storageBuffer16BitAccess-11161`：为 false 时 16 位对象不得处于 `StorageBuffer` 存储类。

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
    uint32_t cells;                  // tile 自身格数/边（网格实际生成 cells + 1 格：外扩带）
    uint32_t lod;                    // 网格步长 = 1 << lod（texel）
    float    height_min;             // 高度界：剔除 + LOD 误差上界
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

### 3.3 裂缝的本质与遮缝手段（**LOD 接缝的正确结论**）

**T 型交点缝是固有几何问题，不能靠采样方式消除。**

- 相邻 LOD 共享一条边时：细侧是折线（穿过多个顶点），粗侧是弦（只连两端点）。
  两者只有在该边上的高度函数为**线性**（平坦或恒定坡度）时才重合。
- "2 的幂步长 + 同一高度函数"能保证的是**顶点级重合**：粗格点位置是细格点子集 →
  取到同一个 texel → 同一个高度值。这**不等于**边内部重合。

三种工程手段：

| 手段 | 说明 | 代价 | 本方案 |
|---|---|---|---|
| **a. 外扩重叠** | 每个 tile 的网格向 −X/−Y 各多生成 1 格；边界线两侧各有连续表面跨过 ⇒ 缝被**覆盖**（不是堵住） | 顶点 81→100、图元 128→162（上限内）；**无第二套索引逻辑** | **V3b 采用** |
| b. 向下裙边（skirt） | 边缘额外向下挤一圈三角形，把缝堵住 | 独立索引逻辑 + 绕序处理 + 裙深参数 + **需按裂缝深度标定** | 不采用（2026-09-28 指示） |
| c. 顶点形变（geomorph / CDLOD morphing） | 细侧过渡带顶点向粗侧弦插值 | 需要 morph 因子与过渡带判定 | 可选优化 |

**双线性采样的真实作用**：把"高度函数"与"网格分辨率"解耦（网格点落在 texel 之间时避免阶梯），
并让不同 LOD 在**任意**位置取到同一函数值 —— 这使裂缝**有界且可量化**（是 a/b/c 的前提）。
在"2 的幂 + texel 对齐"时它退化为直接取纹素（fraction = 0），因此**它不是消裂手段**。

**外扩重叠的四条要点**

1. **ε 只需避开共面**：外扩带顶点高度 −= `overlap_epsilon × 格世界尺寸`（默认 `overlap_epsilon = 1e-3`）。
   ε **不是**裙深：它不需要覆盖裂缝深度 ⇒ **不需要任何裂缝深度测量与标定**（相对裙边最大的稳定性收益）。
   唯一要求：ε 在该距离上大于深度精度。
2. **两侧对称外扩 ⇒ 不可能有孔洞**：一条边界线既是"本 tile 网格的边缘"、又是"邻居网格的内部"
   （邻居也向本侧外扩了 1 格）。
3. **平坦区无 z-fighting**：外扩带下沉 ε 后落在邻居表面之下；起伏区可能穿透邻居表面 ≤ LOD 误差 ⇒
   1 格宽的细薄片（**不是孔洞**，视觉可接受）。
4. **外扩采样天然合法**：外扩格的 texel 落在邻居范围内，高度缓冲连续 ⇒ **不需要 halo**；只有世界边界要 clamp。

**判据（V3b，可自动判定）**：边界带无孔洞；平坦区多帧截图一致（无 z-fighting）；
**关闭外扩（`flags` 位0）能复现 V3 的可见裂缝**；顶点 ≤100 / 图元 ≤162。
第 1 条必须与第 3 条配对："开着没缝、关掉有缝"才算有效证据。

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

### V2 — 多 tile 直发（**不剔除**）

**内容**：CPU 合成 tile 表（`BuildTileList(view, lod_policy)` **纯函数**，V5 原样搬进 compute）；
**直发**：每个 tile 一次 `vkCmdDrawMeshTasksEXT(groups, 1, 1)`，tile 号经 push constant 下发；
**不做剔除**（2026-09-28 指示：先不处理 Frustum）。tile 表按帧槽多份。

**判据**：全部 tile 出图、拼合无重复/空洞（边界高亮材质）；draw 数 == tile 数；所有 tile 同 `lod`。

### V3 — LOD（**仍不遮缝**）

**内容**：`BuildTileList` 引入 LOD（相机距离 / 屏幕空间误差 + `height_min/max` 极差）；
构建 per-tile min/max 金字塔（四叉树）供 LOD 误差度量与地平线遮挡；「2 的幂步长」纪律 + 对角线约定统一。

**判据**：**顶点级重合**（共享边粗格点高度差 == 0）；
**"能看见裂缝"是预期结果**（同相机截图存档，作为 V3b 的对照）；拉远后 `lod` 上升、总组数下降。

### V3b — 外扩重叠遮缝

**内容**：网格格域 = `cells + 1`（向 −X/−Y 各外扩 1 格），外扩带高度 −ε（§3.3）；
`flags` 位0 可关闭外扩（对照用）。**不做向下裙边、不引入裙深参数。**

**判据**：见 §3.3 结尾四条。

### V4 — ICB 化（CPU 生成 Indirect Command Buffer）

**内容**：CPU 把命令写进 `VkDrawMeshTasksIndirectCommandEXT` 数组（每条 `{N²,1,1}`）+ count 缓冲；
**一次** `vkCmdDrawMeshTasksIndirectCountEXT` 覆盖全部 tile，mesh 里 `tile = TBUF.t[gl_DrawID]`
（命令序 = 表行序）。**仍不剔除。**

**判据**：与 V3b **逐像素一致**；命令数 == tile 数；绘制调用数降为 1；`maxDrawCount` 与 count 实测值核对。

### V5 — GPU-Driven（compute 生成，**不用 Task Shader**）

**内容**：compute 写 tile 表 + 命令数组 + `atomicAdd` 写 count；`vkCmdFillBuffer` 归零 count；
`vkCmdDrawMeshTasksIndirectCountEXT` 一次提交全部 tile；每帧槽独立缓冲。CPU 只提交一次 compute + 一次绘制。
**仍不剔除**（只把生成搬家，保证与 V4 行为等价）。

**三条硬约束**：
1. count 归零 → append → 消费 的批次顺序正确，且归零目标与上一帧消费**不同帧槽**；
2. tile 表 / 命令数组的 usage 必须含 `INDIRECT_BUFFER`（+ `SHADER_DEVICE_ADDRESS`）；
3. 屏障用 `VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT` / `DRAW_INDIRECT`（sync2 写法更清晰）。

**判据**：与 V4 **逐像素一致**；CPU 侧零逐 tile 写入；绘制调用数 = 1 且与 tile 数无关。

### V6 — CS 内 Frustum 剔除

**内容**：compute 里用 tile AABB = `texel_origin + tile_span + height_min/max` 做视锥剔除（+ 地平线 / 背向），
剔除掉的 tile 不 append 命令（count 变小）。这是第一个真正减少工作量的阶段。

**判据**：可见集与 CPU 参考实现逐 tile 一致；视野外 tile 不出现在命令数组里。

### V7 — 可选实验（独立工程的独有价值）

| 实验 | 方法 | 价值 |
|---|---|---|
| 纹理路线 A/B | 高度改 `R16_UNORM` 纹理 + sampler（给 binding 加 `VK_SHADER_STAGE_MESH_BIT_EXT`），硬件双线性 vs 手动双线性 | 量化采样成本与代码复杂度（ULRE 里做不了） |
| 稀疏驻留 | `VK_IMAGE_CREATE_SPARSE_BINDING_BIT` + HEAP 内存 + `vkQueueBindSparse` + 页/居民化 | 本机 `sparseResidency*` 全 true；验证"巨大虚拟高度图"路线 |
| 网格着色器 LOD 变体 | 每 tile 网格分辨率随距离变化（非幂等步长） | 需回到双线性采样 + 定量评估 |
| Task Shader 对照 | 仅作开销对照（**不进主线**）：`EmitMeshTasksEXT` 分级分发 vs compute 生成 | 回答"Task 是否值得"；本机 `taskShader = true` |

（首版此处「V4 路 1 = Task Shader 为主」已按 2026-09-28 指示取消；剔除也从 V2 移到 V6。）

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
| 多 tile 直发（V2）/ ICB（V4） | 命令数组 dump + pipeline statistics | 全部 tile 出图、拼合无重复/空洞；命令数 == tile 数；绘制调用数 = 1（V4） |
| LOD 顶点重合 | compute/CPU 自检 | 共享边粗格点高度差 == 0 |
| **外扩遮缝** | 固定相机截图 + 多帧比对 + 关闭外扩对照 | 边界带无孔洞；平坦区无 z-fighting；**关掉能复现裂缝** |
| 剔除正确性（V6） | 命令数组 dump | 视野外 tile 不出现在命令数组里（count 变小） |
| 性能 | timestamp query（period 52.08 ns） | 记录 V1→V6 的 mesh 阶段耗时/三角数/命令数 |
| 回归 | 同相机同位置截图 | 每阶段与前一阶段在非变化区域逐像素一致（V3→V3b 只该在边界带变化） |

---

## 6. 风险与开放问题

| # | 风险 | 处理 |
|---|---|---|
| R1 | **mesh 输出是四重预算**（vertices / primitives / memory / components），只盯 `maxMeshOutputVertices` 会在别处失败 | 按规范下限设计（256 顶点/256 图元），管线创建即失败便于早发现；本机实测见 §2.2 |
| R2 | 用户 varying 未写 `layout(location)` → `'location' : SPIR-V requires location for user input/output` | 实测踩过：mesh 阶段的 `out` 数组成员必须显式 location |
| R3 | 顶点输出写入方式 | 用户 varying 用 `varying[i] = ...` 直接数组索引（`gl_MeshVerticesEXT[]` 只含 `gl_Position` 等内建）；已编译验证 |
| R4 | BDA 依赖 `bufferDeviceAddress + scalarBlockLayout + shaderInt64` | 三项作为硬性设备门槛在启动时断言；不满足则退描述符版 |
| R5 | `shaderFloat64 = false`（本机） | 世界坐标精度只能靠 CPU double + 相机相对；不要指望 shader 内 double |
| R6 | 帧槽竞态（本帧 compute 写 / 上一帧还在读） | 从 V2 起就按帧槽多份化，别等到 V5 |
| R7 | 外扩带与邻居表面共面 ⇒ z-fighting（平坦地形最明显）；起伏处穿透邻居表面 ⇒ 1 格宽细薄片 | 外扩带 −ε + 两侧对称外扩（§3.3）；判据含「平坦区多帧一致」；薄片不是孔洞，接受 |
| R8 | 上 sparse 的诱惑 | 硬件支持但工程量独立（HEAP/page size/mip tail/绑定队列）；列为 V5 实验，不进主线 |
| R9 | 平台差异（NV 的 256 顶点上限、不同 `maxPushConstantsSize`） | 全部上限从 `vkGetPhysicalDeviceProperties2` 查询后决定分组，不写死 |
| R10 | 高度缓冲与 tile 表混在一张大 buffer 里导致对齐全乱 | 一缓冲一用途；高度只做"基地址 + 整数索引"（§2.3 寻址规则） |
| R11 | 外扩带来每边界 1 格的重叠绘制（overdraw） | 开发期接受；若日后成瓶颈，只让「较粗一侧」外扩（需邻接 LOD 信息） |
| R12 | 相位梯里「V6 剔除」之前误把剔除当成早期目标 | 2026-09-28 指示：V2–V5 **一律不剔除**，只在 V6 做；每阶段判据里不得出现剔除断言 |

---

## 7. 里程碑与回滚

- 分支 `terrain-experiment`，每阶段 tag：`v1-single-tile` / `v2-tiles` / `v3-lod` / `v3b-overlap` / `v4-icb` / `v5-gpudriven` / `v6-cull`。
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
- 生成的 SPIR-V capability：`Int64` / `MeshShadingEXT` / `PhysicalStorageBufferAddresses` / `StorageBuffer16BitAccess`（16 位高度）；
- 每个物理指针访问都带 `Aligned` 操作数（16 位 ⇒ 2，tile 行 ⇒ 4），见 §2.6；
- **16→32 位必须两级转换**：`float(uint(h))`，直接 `float(h)` 会被 glslc 拒（`'constructor' : can't convert`）。

下面这份 `terrain.mesh` 就是本机实测通过的那一份（`glslc --target-env=vulkan1.3` → SPIR-V **9608 B**；
`spirv-val` 零 error；capability `MeshShadingEXT` / `PhysicalStorageBufferAddresses` / `Int64` / **`StorageBuffer16BitAccess`**；
`OutputVertices 128` / `OutputPrimitivesEXT 192`（声明上限 ≥ 实产 ≤100/≤162）；对齐见 §2.6：
`OpLoad %ushort … Aligned 2`（高度）、`OpLoad %TerrainTile … Aligned 4`、`ArrayStride 2 / 32`）：

```glsl
#version 460
#extension GL_EXT_mesh_shader : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_16bit_storage : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(local_size_x = 64) in;
layout(triangles, max_vertices = 128, max_primitives = 192) out;

struct TerrainTile
{
    uint  texel_origin_x;
    uint  texel_origin_y;
    uint  cells;                  // tile 自身格数/边（ULRE 版恒 = 8 * groups_per_side）
    uint  lod;                    // 格步长 = 1 << lod（texel）
    float height_min;
    float height_max;
    uint  page_index;
    uint  flags;
};

layout(push_constant) uniform PC
{
    mat4     vp;
    uint64_t addr_heights;
    uint64_t addr_tiles;
    uint     tile_index;
    uint     heights_stride;
    float    texel_world_size;
    float    height_scale;
    float    overlap_epsilon;      // 外扩带下沉量（× 本 tile 格世界尺寸，默认 1e-3）
    uint     world_texels;         // 世界高度缓冲边长（clamp 用）
} pc;

// 高度：原生 16 位 ⇒ Aligned 必须是 2（VUID-06314：被指向类型最大标量宽度）
layout(buffer_reference, scalar, buffer_reference_align = 2) readonly buffer HeightRef { uint16_t data[]; };
// tile 行结构最大标量 4B ⇒ Aligned 4
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer TileRef   { TerrainTile t[]; };
#define HBUF HeightRef(pc.addr_heights)
#define TBUF TileRef(pc.addr_tiles)

layout(location = 0) out vec2 vUV[];                             // 必须显式 location

float SampleHeightClamped(int tx, int ty)
{
    const int m = int(pc.world_texels) - 1;
    tx = clamp(tx, 0, m);
    ty = clamp(ty, 0, m);
    return float(uint(HBUF.data[uint(ty) * pc.heights_stride + uint(tx)]));  // 16→32 必须两级转换（§2.6）
}

void main()
{
    const TerrainTile tile = TBUF.t[pc.tile_index];
    const uint N  = (tile.cells + 7u) / 8u;                      // 组数/边
    const uint kx = gl_WorkGroupID.x % N;
    const uint ky = gl_WorkGroupID.x / N;

    // 本组格域：自身格数（末组可能不足 8）+ 外扩 1 格（仅每轴最后一个组）
    const uint cols = uint(min(int(tile.cells) - int(kx) * 8, 8)) + ((kx == N - 1u) ? 1u : 0u);
    const uint rows = uint(min(int(tile.cells) - int(ky) * 8, 8)) + ((ky == N - 1u) ? 1u : 0u);
    const int  cx0  = int(kx) * 8 - 1;                           // 格域起点（-1 = 外扩格，故用 int）
    const int  cy0  = int(ky) * 8 - 1;

    const uint vpc  = cols + 1u;                                 // 顶点列数（2..10 ⇒ 顶点 ≤ 100）
    const uint vtot = vpc * (rows + 1u);
    const uint ptot = cols * rows * 2u;                          // 图元 ≤ 162

    SetMeshOutputsEXT(vtot, ptot);                               // 组内一致（由 kx/ky 决定，不看线程）

    const uint  li = gl_LocalInvocationIndex;
    const float cs = pc.texel_world_size * float(1u << tile.lod); // 本 tile 的格世界尺寸

    for (uint i = li; i < vtot; i += 64u)
    {
        const uint vx = i % vpc;
        const uint vy = i / vpc;
        const int  cx = cx0 + int(vx);
        const int  cy = cy0 + int(vy);

        const int tx = int(tile.texel_origin_x) + (cx << int(tile.lod));
        const int ty = int(tile.texel_origin_y) + (cy << int(tile.lod));

        float hw = SampleHeightClamped(tx, ty) * pc.height_scale;
        if (cx < 0 || cy < 0)                                    // 外扩带：压 ε 藏在邻居表面之下（§3.3）
            hw -= pc.overlap_epsilon * cs;

        const vec3 world = vec3(float(cx) * cs, float(cy) * cs, hw);
        gl_MeshVerticesEXT[i].gl_Position = pc.vp * vec4(world, 1.0);
        vUV[i] = vec2(float(cx), float(cy));
    }

    // 图元：格 (cxl,cyl) → 顶点 (cxl,cyl)(cxl,cyl+1)(cxl+1,cyl)(cxl+1,cyl+1)
    // 对角线约定固定 (TL,BL,TR) + (TR,BL,BR)，全局一致
    for (uint i = li; i < ptot; i += 64u)
    {
        const uint cell = i >> 1u;
        const uint cxl  = cell % cols;
        const uint cyl  = cell / cols;
        const uint v00  = cyl * vpc + cxl;
        const uint v01  = (cyl + 1u) * vpc + cxl;
        const uint v10  = v00 + 1u;
        const uint v11  = v01 + 1u;
        gl_PrimitiveTriangleIndicesEXT[i] = ((i & 1u) == 0u)
            ? uvec3(v00, v01, v10)
            : uvec3(v10, v01, v11);
    }
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

**V3b 外扩重叠（已并入上方骨架）**：

- 组 `k` 的格域起点 `cx0 = 8k − 1`（**`−1` 就是外扩格**，故格号走 `int`）；
- `cols/rows = min(cells − 8k, 8) + (k == N−1 ? 1 : 0)` ⇒ 每轴最后一个组多 1 格；
- 外扩格（`cx < 0 || cy < 0`）高度 −= `overlap_epsilon × 格世界尺寸` ⇒ 藏在邻居表面之下；
- **无第二套索引逻辑、无绕序翻转**（`(TL,BL,TR)+(TR,BL,BR)` 全局一致）—— 这是相对向下裙边的主要简化；
- 高度走**原生 16 位**：`buffer_reference_align = 2` + `float(uint(h))` 两级转换（§2.6）。

本骨架已用本机 SDK 实测通过：

```bash
glslc --target-env=vulkan1.3 -o terrain.mesh.spv terrain.mesh   # 9608 B
spirv-val --target-env vulkan1.3 terrain.mesh.spv               # 零 error
# capability: MeshShadingEXT / PhysicalStorageBufferAddresses / Int64 / StorageBuffer16BitAccess
# OutputVertices 128 / OutputPrimitivesEXT 192（声明上限 ≥ 实产 ≤100/≤162）
# 对齐: OpLoad %ushort Aligned 2 / OpLoad %TerrainTile Aligned 4 / ArrayStride 2, 32
```

### C++ 侧 push constant 对应结构（104B）

```cpp
struct TerrainPushConstants            // std430 / 标量布局，104B
{
    glm::mat4     vp;                  // 64
    uint64_t      addr_heights;        // 8
    uint64_t      addr_tiles;          // 8
    uint32_t      tile_index;          // 4（直发期用；ICB 期由 gl_DrawID 取代）
    uint32_t      heights_stride;      // 4  texel/行
    float         texel_world_size;    // 4
    float         height_scale;        // 4
    float         overlap_epsilon;     // 4  外扩带下沉量（× 格世界尺寸，默认 1e-3）
    uint32_t      world_texels;        // 4  世界高度缓冲边长（clamp 用）
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
| tile | 绘制单位：`cells × cells` 个自身格（网格另加 1 格外扩带）+ lod；"画在哪、读哪"的唯一权威 |
| 格（cell） | tile 内一个四边形；`lod` 决定它的 texel 跨度（网格按**顶点槽**发射，不再是 1 线程 1 格） |
| 组（workgroup） | 64 线程，覆盖 8 格（每轴最后一组 9 格，含外扩带），产 ≤100 顶点 / ≤162 图元 |
| 裂缝 / T 型缝 | 相邻 LOD 共享边上，细侧折线与粗侧弦的几何偏差 |
| 外扩重叠带 | 每个 tile 向 −X/−Y 各多生成的 1 格网格，与邻居几何重叠以覆盖 LOD 接缝（取代向下裙边） |
| 高度界金字塔 | 每层 tile 的高度 min/max 树，用于剔除与 LOD 误差度量（**不是** mipmap） |
| BDA | `VkDeviceAddress` / GLSL `buffer_reference`，shader 直接用 64 位地址寻址缓冲 |
