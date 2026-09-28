# ULRE 地形渲染实现方案 v2（Compute + Mesh Shader · GPU-Driven · BDA）

> **⚠ 实现请读 `doc/terrain-implementation-handbook.md`**（2026-09-28 重组的实现手册，**唯一开工依据**）：
> 任务卡 T1–T4、可编译 GLSL 骨架（实测 `glslc` + `spirv-val` 通过）、触点行号、判据与验证台、换机自检都在那一份里。
> 两者冲突时**以手册为准**。
>
> **本档 = 推导过程与决策记录**（下面这套论证过程的价值在于"为什么这么定"，实现时不必再读一遍）。

> **版次**：v2 全文重写（2026-09-28）。v1（`doc/terrain-rendering-implementation-plan.md`）写于 2026-09-24，
> 基于已过期的引擎基线，且其中「LOD 接缝不需要裙边」的结论是错的、已被推翻。本版把全部已知输入
> （引擎契约、你的 `doc/Terrain Vulkan 1.4.rtf` 设计意图、源 OpenGL 工程的可取之处）合并成一套
> **可直接开工**的方案；v1 的具体更正记在 §10，不散落在正文里。
>
> **2026-09-28 变更指示**：**不走 Task Shader**，改走 **ComputeShader + MeshShader**；先由 **CPU 生成
> Indirect Command Buffer** 且**暂不做任何剔除**，将来把 tile 表/命令生成迁进 ComputeShader，并在 CS 里做
> Frustum 剔除。本节及 §4.2 / §7 已按此改（U1 覆盖原 RTF 的 Task Shader 决策）。
>
> **同日另两条指示**：**T2 拆成四步**（多 tile → LOD → 外扩遮缝 → ICB 化，§7）；
> **开发期以稳定性为准、不追求性能**（§7 原则表）。另有 **U6 由「向下挤出裙边」改为「与邻居外扩重叠」**
> （§4.1/§5.2）—— 这条连带把「裙深标定」整条判据删掉了：外扩的 ε 只需避开共面，不需要覆盖裂缝深度。
>
> **关联文档**
> - `doc/Terrain Vulkan 1.4.rtf` —— 你的设计意图稿（81 顶点拓扑 / 相机相对——采纳；其 Task Shader 部分已被 U1 覆盖，其「同 workgroup 挤出裙边」已被 U6 改为外扩重叠）
> - `doc/global-addresses-bda-unification-plan.md` —— S1/S2/S3 地址与描述符收敛（现基线的真源）
> - `doc/scene-ubo-bda-migration-handoff.md`、`doc/camera-ubo-removal-plan.md`、`doc/t3-camera-row-slotting-plan.md`
> - `doc/terrain-vulkan-standalone-experiment.md` —— 脱离 ULRE 的独立 Vulkan 实验版（与本档互为对照）
> - `doc/backlog.md` —— 帧资源/槽体系历史与遗留

---

## 0. 设计输入

### 0.1 已定输入 —— 用户决策（U1/U6 为 2026-09-28 的覆盖指示，U2–U5、U7 来自 `Terrain Vulkan 1.4.rtf`）

| # | 决策 | 本版落点 |
|---|---|---|
| U1 | **Compute Shader + MeshShader 两阶段**，**不用 Task Shader**；先由 **CPU 生成 Indirect Command Buffer**、**暂不做剔除**；将来 tile 表/命令生成迁进 CS，**Frustum 剔除在 CS 里做** | §4.2、§7-T2.4/T3/T4 |
| U2 | **9×9 = 81 顶点对应 8×8 格**（顶点复用，不是每格 4 顶点） | §4.1 |
| U3 | **原生 `uint16_t[]` 高度**（`storageBuffer16BitAccess`） | §2.3（本机该 feature = true） |
| U4 | **mesh 阶段中心差分法线**（不用片元 `dFdx/dFdy`，不烘法线图） | §4.3 |
| U5 | **相机相对渲染**（CPU double + relative VP） | §3.4、§2.2 |
| U6 | **外扩重叠遮缝**：每个 tile 向 −X/−Y 各多生成 1 格，与邻居几何重叠以覆盖接缝；**不做向下挤出的裙边**（2026-09-28 覆盖 RTF 原意） | §4.1、§5.2 |
| U7 | **精简 push constant**（不要把矩阵塞进 PC） | §3.1（走地址表 + per-frame 块，PC 不动） |

### 0.2 已定输入 —— 引擎不变量（2026-09-28 实测，违反即编译失败或渲染崩）

| # | 不变量 | 出处 |
|---|---|---|
| E1 | **唯一描述符集 `BINDLESS_SET=0`**；Scene 集已整体退场 | `ShaderLibrary/common/descriptor_macros.glsl` |
| E2 | **材质定义声明的描述符必须为空**（TOML `ubos` 键已删、授权规则表已删） | `e59e65620` |
| E3 | **相机数据只能经 BDA 宏取**：`camera = CameraInfoBufferRef(global_addresses.addr_camera_info).cameras[pc_root.camera_id]`；Camera UBO 已删 | `86535dbdb`、`ShaderLibrary/ubo/scene_ubo.glsl:149` |
| E4 | `pc_root` = `RootAddresses`，**80B**（9×uint64 + 2×uint32，首字段 `addr_global_addresses`）；布局断言按 X 列表自动推导 | `ShaderBufferSources.h:267-278` |
| E5 | 全局地址表按 **`HGL_FRAME_SLOT_TOTAL=8` 帧槽**分份（槽步长 128B、16B 对齐）；**地址在 buffer 物化处注册**，取不到即 fail-fast | `inc/hgl/graph/ubo/GlobalAddresses.h` |
| E6 | 帧槽空间：主帧 `[0,HGL_FRAME_SLOT_MAIN=4)` 与离屏 `[4,8)` **不相交**；ring 深度必须 = 槽总数 | `RenderOptions.h:16-33` |
| E7 | **mesh 阶段不能采样 bindless 纹理**（三 binding 的 `stageFlags` = `FRAGMENT\|COMPUTE`）；高度只能走 BDA 缓冲 | `VKBindlessTextureManager.cpp:42/48/54` |
| E8 | 顶点阶段只有 mesh shader；`VertexPassthrough` 只能表达三角汤 ⇒ 地形必须新增 mesh 模式 | `MeshModeDescriptor.h`、`vertex_passthrough.glsl.tmpl:23-27` |
| E9 | meshgen **没有** task 阶段发射器（只有 topology/defines/资源/stage1-3/body 九槽）——本方案走 CS 路线，故此条不阻塞，仅记录「Task 路线的成本」 | `src/ShaderGen/meshgen/MeshModeDescriptor.h:63-73` |

### 0.3 前次尝试的取舍（不复用，但要知道它们存在）

| 前次产物 | 位置 | 本版态度 |
|---|---|---|
| `src/ShaderGen/3d/M_TerrainGrid.cpp` + `TerrainGridCreateConfig` + `example/Environment/TerrainHeightmap.cpp` | 分支 `devel_45_SceneEventDispatcher`（**不在当前树**） | **不复用**：那是 3D（VS/FS）路线，与「顶点阶段只有 mesh shader + GPU-driven」相悖；其配置结构可参考命名 |
| `ecs/support/terrain/*`（Build/Collect/RenderPipeline/TileBuffer）+ `TerrainTileComponent` | 历史提交（已删除） | 不复用机制，但 §6.1 的四件套外壳照 `ecs/support/line/*` 抄（仍在树里、活的） |
| `ShaderLibrary/position_provider/terrain_grid.glsl`、`compositor/main_terrain_grid.*` | 历史提交（已删除） | 不复用 |
| `ShaderLibrary/sampler.toml` 的 `Terrain` 预设（Linear/Aniso） | **在树里，活的** | T4 加材质贴图时可直接用这个名字 |
| `PositionSourceSpec::TerrainHeightmapGrid` 枚举 | 在树里（空壳，`PrimitiveBatchPipeline.cpp:287-312` 透传） | 保持空壳：本方案走专用管线，不走通用批处理 |

### 0.4 三个待拍板项（各有推荐，不定也能开工）

| 项 | 选项 | 推荐 | 影响 |
|---|---|---|---|
| **D1 地址落位** | (A) 进全局地址表（高度 = 全局字段、tile 表/per-frame 块 = 每帧槽字段）；(B) 加进 `RootAddresses`（`pc_root` 80→**112B**） | **B**（开发期）| **直发期必须传 per-draw 的 tile 索引，而 GLSL 每 stage 只有一个 `push_constant` 块 ⇒ 该索引只能进 `pc_root`**；既然 pc_root 要动，四字段一起走它机制最少（§3.1）。末尾追加默认参数 ⇒ 零调用点改动。T2.4 后可迁回 A（可选优化） |
| **D2 集成路线** | (A) 走材质系统：新增 `MeshShaderMode::TerrainGrid` + `terrain.material.toml`（9 触点）；(B) 地形自持管线 + 自带 GLSL | **A** | B 必须手写 `pc_root` / `camera` 宏声明，**违背单一真源**（生成器才是这些声明的真源）；只有想完全绕开材质系统时才选 B |
| **D3 间接提交形态**（T2.4 起） | (A) 直接上 `DrawMeshTasksIndirectCount`（count 由 CPU 写进 count 缓冲）；(B) 先 `DrawMeshTasksIndirect`（CPU 直接给 drawCount） | **A** | A 在 T3 迁到 CS 时**CPU 提交代码零改动**（只换生产者）；B 到 T3 要改提交点。引擎两条都在（`RenderCmdBuffer` 的 `DrawMeshTasksIndirect` / `…IndirectCount`） |

---

## 1. 目标与非目标

**目标**：MeshShader 内生成地形网格（XY 由索引生成、Z 由高度场取），数据全走 BDA 缓冲；
CPU 侧只提交「画哪些 tile、读哪段高度」；LOD 无缝；最终由 **ComputeShader** 生成 tile 表/间接命令并在其中做 **Frustum 剔除**（GPU-driven）。

**开发期总原则（用户 2026-09-28 指示）**：T1–T2.4 只是开发过程，**以开发稳定性为准，不追求性能**
（具体取舍见 §7 开头的原则表）。性能基线只在 T3/T4 之后才纳入记录。

**非目标（本版不做）**：磁盘页流式、稀疏纹理、材质 splatting/贴图细节、tessellation 路线、
物理/碰撞（`src/JoltPhysics/Assets/terrain*.bof` 与本方案无关）、地形编辑器、剔除与批合并（T4 才做）。

---

## 2. 数据模型

### 2.1 `TerrainTile`（32B，C++ 与 GLSL 逐字段对齐）

```cpp
// inc/hgl/graph/ssbo/TerrainRows.h（新增）
struct TerrainTile                       // 32B
{
    uint32_t texel_origin_x;             // 全局 texel 原点（整数、精确）
    uint32_t texel_origin_y;
    uint32_t groups_per_side;            // tile 内 8×8 格的组数 N（tile 内格数 = 8N）
    uint32_t step_shift;                 // 格步长 = 1 << step_shift（texel）
    float    height_min;                 // 高度界：剔除 + LOD 误差上界
    float    height_max;
    uint32_t page_index;                 // 页索引（v2 恒 0，T4 启用）
    uint32_t flags;                      // 位0：**关闭**外扩（调试/对照用）；位1：需要采样（CPU 剔除结果）
};
static_assert(sizeof(TerrainTile) == 32);
```

**核心不变式（tile 是世界尺寸恒定的正方形）**

```
tile_span_texels = 8 * groups_per_side * (1 << step_shift) = kTileSpan（常量，默认 256）
⇒ groups_per_side = kTileSpan / (8 << step_shift)          // 二者只有一个自由度：step_shift
```

好处三条：

1. **tile 数量只由「世界尺寸 / tile 尺寸」决定，与 LOD 无关** ⇒ tile 表有上界
   （8192 m 世界、256 m tile → 32×32 = 1024 个 × 32B = 32KB），可以一次分配、长期复用；
2. LOD 只改 `groups_per_side`（tile 内发多少个 8×8 组）⇒ 远处 tile 的 `groupCountX = N²` 变小，
   工作量直接下降；
3. tile 跨度为 2 的幂 ⇒ **tile 原点天然按所有可能的步长对齐**（这是粗/细格点重合的硬前提，见 §5.1）。

### 2.2 `TerrainFrameData`（per-frame ring，每帧槽一份）

```cpp
struct TerrainFrameData                  // 96B（16B 对齐）
{
    glm::mat4 relative_vp;               // 相机相对视图投影（CPU double 算好后降精）
    float     camera_world_rel[3];       // 相机相对世界位置（雾/距离/远景用）
    float     texel_world_size;          // 一个 texel 的世界尺寸（米）
    float     height_scale;              // 高度单位 → 世界单位
    float     overlap_epsilon;           // 外扩带下沉量（× cell_world_size，§5.2；只需避开共面）
    uint32_t  tile_count;                // 本帧有效 tile 数
    uint32_t  _pad[3];
};
static_assert(sizeof(TerrainFrameData) == 96);
```

**为什么必须有它**（而不是直接用引擎的 `camera.vp`）：见 §3.4。

### 2.3 高度缓冲

- 格式：**原生 `uint16_t[]`**（`GL_EXT_shader_16bit_storage` + `storageBuffer16BitAccess`，本机 = true）。
- 组织：**一张连续的世界高度缓冲**（不是每 tile 一张）。理由：mesh 阶段的中心差分要读 `±step` 个 texel，
  在 tile 边缘必然越界；只要缓冲覆盖整个世界，越界读就是合法的邻居采样 —— **v2 不需要 halo**，
  只有**世界边界**需要 clamp（§4.5）。
- 页（`page_index`）只是**将来**的流式单位，T4 才启用；届时页需要 halo（= 最大 step）。
- 上传：静态数据一次性 staging → `vkCmdCopyBuffer`（引擎的**环形 Staging 池**已可用，`4170b23a9`）。
- 对齐/尺寸的硬约束与容量天花板见 **§2.6**（buffer 不是纹理；2 的幂加在 LOD 上）。

### 2.4 min/max 高度金字塔

每个 tile 一行 `{height_min, height_max}`，按 quadtree 逐级合并。用途：
(a) 视锥/地平线剔除的 Z 界；(b) LOD 误差度量里的高度幅度项；(c) 量化 LOD 误差上界
（T4 若要上顶点形变或收紧判据时用）。**不是 mipmap**
（mip 会改变几何函数，破坏「粗/细格点同值」的前提）。

### 2.5 两级（页 / tile）的收束

v1 提过「页 + tile」两级。v2 **只做 tile**，把「页」降级为 `TerrainTile` 里的一个字段：
现在 `page_index` 恒 0、高度缓冲单一、无流式；T4 再多页 + 驻留表 + 非驻留页降级（跳过或降 LOD），
届时页 = 高度缓冲的一段整数元素偏移。

### 2.6 对齐与尺寸约束（**走 buffer 不是纹理；2 的幂加在 LOD 上，不在数据尺寸上**）

**结论先行**：mesh 顶点数不是瓶颈（≤100 顶点 / ≤162 图元，对上限 1024 与规范下限 256 都有一倍以上余量）；
"超大高度图要对齐"是对的，但**纹理那套对齐规则一条都不适用**，真正需要 2 的幂的是 **LOD 的格步长与 tile 跨度**，
不是高度数据（或纹理）的尺寸。

**为什么高度走 buffer 而不是纹理（ULRE 里没得选，句句有据）**

1. **ULRE 硬约束 E7**：bindless 纹理集的三个 binding `stageFlags` 全是 `FRAGMENT|COMPUTE`
   （`VKBindlessTextureManager.cpp:42/48/54`，第二组 `:220/226/232` 同）⇒ **mesh 阶段采样不到纹理**。
   而我们的 Z 必须在 mesh 阶段取到（§4.1：XY 由索引生成、Z 由高度场取）⇒ **只能读 buffer**。
2. **整数域**：`R16_UINT` **不能硬件双线性**、不能线性 blit；要硬件插值就得存 UNORM
   （归一化丢高度语义、还要量化还原）。走 buffer 时顶点落在 texel 上（fraction = 0），
   手动 4 读 + lerp 实际退化为单次读，滤波成本可以忽略。
3. **少一整套状态**：无 sampler（filter / address mode）、无 image layout 转换（纹理要在 CS 写/读还得加 barrier）、
   越界读就是普通索引加减（中心差分读 ±step 天然合法，§4.5）、分块不受 `maxImageArrayLayers` 限制。
4. **地址稳定**：BDA 地址在 T3（CS 写表）后不变，compute 与 mesh 用同一地址读，无需重绑。

代价（别只看好处）：没有硬件滤波 / 各向异性 / 自动归一化（高度由 shader 乘 `height_scale`），
且 stride 与对齐要自己管（本节 (1)）。**与"不做 mipmap"是两件事**：mip 是另一个高度函数、
破坏粗/细同值前提（§2.4），和走 buffer 还是纹理无关。


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

## 3. 地址与参数通路（★ 本版与 v1 差异最大的地方）

### 3.1 地形四字段走 `pc_root`（推荐 D1-B：开发期最稳）

| 数据 | 变化频率 | 落位 | GLSL |
|---|---|---|---|
| 高度缓冲（世界一张） | 静态 | `pc_root.addr_terrain_heights` | `#define terrain_heights TerrainHeightRef(pc_root.addr_terrain_heights)` |
| tile 表（每帧重建） | 每帧 | `pc_root.addr_terrain_tiles` | `#define terrain_tiles TerrainTileRef(pc_root.addr_terrain_tiles)` |
| per-frame 块（§2.2） | 每帧 | `pc_root.addr_terrain_frame` | `#define terrain_frame TerrainFrameRef(pc_root.addr_terrain_frame)` |
| **本 draw 的 tile 号** | **每 draw** | `pc_root.terrain_tile_index` | 直接读 `pc_root.terrain_tile_index`（T2.4 起由 `gl_DrawID` 取代） |

改动面：`HGL_ROOT_ADDRESSES_FIELD_LIST` 追加 **5 行**（3×uint64 + 1×uint32 + 1×uint32 补齐 ⇒ `pc_root` 80B → **112B**，
布局断言按 X 列表自动推导；**单个尾部 u32 会因尾填充破坏 `sizeof == Σsizes` 断言，必须成对**），`PushRootAddresses` **在参数列表末尾追加** `uint32_t terrain_tile_index = 0`。
**末尾 + 默认值 = 现有 3 个调用点一行都不用改**（与 S1 那次"把 `addr_global_addresses` 插进第 4 位"
是两回事，见 §9-R1）。

**GLSL 侧没有手写块要同步**（这点与改全局地址表**相反**）：`pc_root` 的 GLSL 声明由生成器遍历同一张
X 列表发射（`MeshShaderHeaderGen.h:28`、`MaterialShaderEmitter.cpp:510`），大小/偏移断言也用同一张表
（`ShaderBufferSources.h:316/322`）⇒ 改 C++ 一处，GLSL 与断言自动跟随。本方案只需要新增**本模式自己的**
`buffer_reference` 声明与 `#define`（`ShaderLibrary/vertex/s1_terrain_grid.glsl`，照 `s1_text_char_quad.glsl`），
**不需要动 `ShaderLibrary/ubo/scene_ubo.glsl`**（那是 `GlobalAddressesRef` 的手写真源，本方案不碰它）。

**为什么不走全局地址表（D1-A）**：那条路要多一整套机制——帧槽取址、物化处注册、取不到即
`result -4`、C++↔GLSL 对表门——而开发期只需要"能画对"。一条 pc_root 承载全部四个字段，
**机制最少、可见性最高、出问题一眼定位**。

### 3.2 两条规则的适用边界（别搞混）

- 地址随 **push constant（或命令缓冲）** 走 ⇒ **地址本身的时序**由命令缓冲负责
  （等价于 S2 文档那句"绑定时代每帧换绑定由命令缓冲自带时序"）；
- **但缓冲内容的竞态不因此消失**：tile 表 / per-frame 块 / 命令缓冲仍是"每帧重建"，
  必须按 `HGL_FRAME_SLOT_TOTAL` 做成 ring，否则本帧写入会踩到上一帧还在消费的数据（§9-R8/R13）。
- T2.4 之后 `terrain_tile_index` 由 `gl_DrawID` 取代、字段退居调试用途；是否把三个地址迁回全局地址表
  （省 24B/批次）**留作可选优化，不进开发期主线**。

### 3.3 三条硬规矩（S2/S3 用事故换来的）

> **与 D1-B 的关系（2026-09-28）**：这三条都围绕**全局地址表**。本方案定为 D1-B（地形四字段走 `pc_root`，§3.1）
> ⇒ **不新增该表字段**，规矩 1、2 **在本方案里不触发**；规矩 3（门核验纪律）**照旧必须执行**。
> 若日后把地形地址迁回全局地址表（T3 之后可选，§3.1 的"可选回迁"），三条立即全部生效。

1. **地址在 buffer 物化处注册**。只在「每帧同步函数」里注册 ⇒ 某条路径没跑到 ⇒ 地址恒 0 ⇒
   shader 解引用 0 基址 ⇒ `vkQueueSubmit2 failed with result -4` = **设备丢失**，
   而校验层只报次生错误（fence in use / semaphore 已 signal）。要求：注册点 + fail-fast +
   一行 `terrain heights addr=0x… 入表` 日志。
2. **C++ 表结构与 GLSL 引用结构同批改**（顺序/数量/名字全一致）。本方案对应的等价风险落在 `pc_root` 上，
   但**已由生成器消除**：GLSL 块与布局断言都遍历同一张 X 列表（§3.1、§6.2 触点 8）⇒ 无"漏改一行"的空间。
3. **每个渲染门单独 grep `result -4`**，且跑前 `tasklist` 核验无残留 exe（否则跑的是旧二进制、
   结论全假）、跑完立即 `taskkill /F /IM`（**单斜杠**，双斜杠在本环境会被原样传给 taskkill）。

### 3.4 相机相对：为什么不能直接用 `camera.vp`

- `camera` 宏里的 `vp` 是**绝对**视投影矩阵；引擎的 camera-relative **尚未启用**
  （`src/SceneGraph/camera/Camera.cpp:64-65`：视图矩阵平移归零与 `pos=0` 暂不启用，
  等 `TransformAssignmentBuffer::SetCameraOffset` 完整接入）。
- 相机相对渲染要求「顶点用相对坐标 + 矩阵不含相机平移」成对出现。既然引擎不给相对矩阵，
  就**自己算一份**：CPU 用 `double` 累加相机世界位置 → 得相对 VP → 写进 `TerrainFrameData.relative_vp`
  → shader 里顶点用相对相机的坐标。`camera.vp` 仍可用于需要绝对量的场合（远景/天空）。
- 精度纪律：世界坐标只在 CPU 侧以 double 出现；shader 里只出现 tile 局部小坐标与相对坐标
  （本机 `shaderFloat64 = false`，shader 内没有 double 兜底）。

---

## 4. 着色器三层

### 4.1 拓扑与预算（U2 + U6：9×9 顶点复用 + 外扩重叠带）

```
一个 tile（N = groups_per_side，格步长 = 1 << step_shift）

  tile 自身面积   = 8N × 8N 格            （= kTileSpan texel，不变式见 §2.1）
  网格格域        = 8N + 1 格 / 边        ← 向 −X / −Y 各外扩 1 格（"与邻居重叠"格式，U6）
  网格顶点域      = 8N + 2 顶点 / 边

一个 mesh workgroup（local_size_x = 64）负责 8 格：
  组数 = N²/tile（与 v1 相同）；每轴**最后一个组**负责 9 格（多出的那 1 格就是外扩带）

  最坏情形（角上那个组：9×9 格）
    顶点 10×10 = 100      （内部组仍是 9×9 = 81）
    图元 9×9×2 = 162      （内部组仍是 8×8×2 = 128）
  ────────────────────────────
  ⟶ 两个上限都是 256，本机（1024）与 NV 下限设备都成立
```

**遮缝原理**：边界线两侧**各有连续表面跨过**（本 tile 向该侧外扩 1 格、邻居也向本 tile 侧外扩 1 格）
⇒ 缝不是被"堵住"的，而是几何上被覆盖、根本不存在（§5.2）。

- **格号是 `int`**：组 `k`（每轴）覆盖格 `[8k − 1, 8k − 1 + cols)`，`cols = 8 + (k == N−1 ? 1 : 0)`；
  格号 `−1` 即外扩格（位于自身面积之外，落到邻居的最后一格上）。
- 顶点槽索引：`v = vy * (cols + 1) + vx`（`cols, rows ∈ {8,9}` ⇒ 槽位 ≤ 100，无紧凑化处理）。
- 图元：格 `(x,y)` → `(TL,BL,TR) + (TR,BL,BR)`，槽位 `(y*cols + x)*2 + {0,1}`；
  **无绕序翻转、无第二套索引逻辑、无第二套几何类别** —— 这是外扩相对"向下裙边"的主要简化。
- 顶点发射：`for (i = gl_LocalInvocationIndex; i < (cols+1)*(rows+1); i += 64)`（≤100）。
- `SetMeshOutputsEXT(vcount, pcount)` 取**组内一致**的运行期值（组间可不同：8/9 格）——
  比 v1 的"边缘组全组一致 + 紧凑槽位"陷阱简单得多，但仍不是编译期常量，故列进 §10#10 与 §9-R3。
- 外扩带顶点高度 = 采样高度 − `overlap_epsilon × cell_world_size`（只有格号 `−1` 的那一圈）。
- 调试开关：`TerrainTile.flags` 位0 关闭外扩 ⇒ 精确复现 T2.2 的可见裂缝（§5.2 判据 3 的对照）。

### 4.2 命令生成与提交（U1：CPU → Compute 的迁移面）

**两个阶段**：命令/表生成（现在 = CPU，将来 = compute）→ 一次间接绘制调用的 mesh 阶段。

**引擎现成件（全在树里，无需新造）**

| 用途 | API | 出处 |
|---|---|---|
| 命令缓冲（CPU 写 / CS 写） | `VulkanDevice::CreateIndirectMeshTaskBuffer(cmd_count, BufferAllocPolicy::Auto, name, extra_usage, sm)` → `IndirectMeshTaskBuffer`（`WriteCmd` / `MapCmd` / `Flush` / `GetVkBuffer`） | `inc/hgl/vk/buffer/IndirectCommandBuffer.h:61-68`、`src/Vulkan/buffer/IndirectCommandBuffer.cpp:88` |
| count 缓冲 | `Device::CreateDrawCountBuffer(...)`（`INDIRECT\|STORAGE\|TRANSFER_DST\|SHADER_DEVICE_ADDRESS`） | `inc/hgl/vk/VKDevice.h:384-391` |
| 提交 | `RenderCmdBuffer::DrawMeshTasksIndirect / DrawMeshTasksIndirectCount`（引擎**强制** multiDrawIndirect，无逐条退化） | `src/Vulkan/VKCommandBufferRender.cpp:451-472` |
| 每条命令独立寻址 | 间接调用的 `gl_WorkGroupID` **每条命令都从 0 开始**，`gl_DrawID` 是命令序 ⇒ 二者组合即「哪个 tile 的哪一组」 | 同上（引擎 material 路径同此约定） |
| 每条命令索引自己的数据 | **`gl_DrawID`**（引擎既有约定：**命令序 = 表行序**；模板"恒发 `rows[gl_DrawID]` 加载点"；本机 `shaderDrawParameters = true`） | `PipelineMaterialRenderer.cpp:39/51-65/87`、`MeshShaderHeaderGen.h:20` |

**命令格式**（引擎约定：`{groupCountX = 组数, groupCountY = 实例数, groupCountZ = 1}`）

```
cmd[i] = { groupCountX = N*N, groupCountY = 1, groupCountZ = 1 }     // N = tile[i].groups_per_side
```

- 约束：`maxMeshWorkGroupCount` 每维 **65535**（本机实测）⇒ 展平成 X 后要求 `N² ≤ 65535`，
  即 `N ≤ 256`；本方案 `N ≤ 32`（`kTileSpan=256, step_shift=0 → N=32`），**余量充足**。
  `maxMeshWorkGroupTotalCount = 4194304`（本机）。
- 直发期（T2.1–T2.3）：`tile = terrain_tiles.t[pc_root.terrain_tile_index]`，每 tile 一次 draw；
  `(gx0, gy0)` 由 `gl_WorkGroupID.x` 对 `N` 取模/整除得到。
- ICB 期（T2.4 起）：`tile = terrain_tiles.t[gl_DrawID]`（**命令序 = 表行序**），一次间接调用覆盖全部 tile，
  per-tile push 退休。

**提交：直发期（T2.1–T2.3）**

```cpp
for (uint32_t i = 0; i < tile_count; ++i)
{
    PushRootAddresses(cmd, dev, layout, addr_global_addresses, mesh_draw_params, /*…*/
                      camera_id, /* terrain_tile_index = */ i);
    cmd->DrawMeshTasks(tiles[i].groups_per_side * tiles[i].groups_per_side, 1, 1);
}
```

（draw 数 = tile 数，开发期接受；这是 §7「开发期原则」里"参数传递走看得见的通道"的落点。）

**提交：ICB 期（T2.4 起）**

```cpp
cmd->DrawMeshTasksIndirectCount(cmd_buf,
        mesh_tasks->GetVkBuffer(), 0,
        count_buffer->GetBuffer(), 0,
        max_draw_count,                        // ≥ 本帧 tile 数
        sizeof(VkDrawMeshTasksIndirectCommandEXT));
```

**迁移到 ComputeShader（T3）**：只换"谁来写"——CS 写 tile 表 + 命令 + `atomicAdd` 写 count，
CPU 侧提交代码（上面这段）**一字不改**。届时补齐：
count 每帧归零（`vkCmdFillBuffer` 或专用 workgroup）→ 屏障（`COMPUTE_SHADER`/`SHADER_WRITE`
→ `DRAW_INDIRECT`/`INDIRECT_COMMAND_READ`）→ 命令/表缓冲按 `HGL_FRAME_SLOT_TOTAL` 的 ring 分份。

**Frustum 剔除（T4）**：现在**不做**（U1 明确）；将来在 CS 里做，判据用 tile 的
`texel_origin + kTileSpan + height_min/max` 组成 AABB —— `height_min/max` 字段 §2.1 已预留，
金字塔（§2.4）也为此准备。

### 4.3 Mesh Shader 主体（整数索引 + 中心差分法线 + 外扩带）

```glsl
// uv/texel 全整数，float 只在最后一步出现
const uint N    = t.groups_per_side;
const uint kx   = gl_WorkGroupID.x % N;                         // tile 内组位（X）
const uint ky   = gl_WorkGroupID.x / N;                         // tile 内组位（Y）
const uint cols = 8u + ((kx == N - 1u) ? 1u : 0u);              // 8 或 9（末尾组含外扩带）
const uint rows = 8u + ((ky == N - 1u) ? 1u : 0u);
const int  cx   = int(kx) * 8 - 1 + vx;                         // 格号（-1 = 外扩格 ⇒ 用 int）
const int  cy   = int(ky) * 8 - 1 + vy;
const uint step = 1u << t.step_shift;
const bool ext  = (cx < 0) || (cy < 0);                         // 外扩带（自身面积之外）
const int  tx   = int(t.texel_origin_x) + (cx << t.step_shift);
const int  ty   = int(t.texel_origin_y) + (cy << t.step_shift);

float h = ReadHeight(tx, ty);                                   // uint16 → float（世界边界 clamp，§4.5）
if (ext) h -= terrain_frame.overlap_epsilon * cell_world_size;  // 只压外扩带：避开与邻居共面（§5.2）
// 中心差分法线（U4）：读 ±step 的邻居，世界边界处 clamp
float hl = ReadHeight(tx - int(step), ty), hr = ReadHeight(tx + int(step), ty);
float hd = ReadHeight(tx, ty - int(step)), hu = ReadHeight(tx, ty + int(step));
vec3 n = normalize(vec3((hl - hr) * height_scale, (hd - hu) * height_scale, 2.0 * cell_world_size));
// 位置：相对相机的世界坐标 → relative_vp
vec3 rel = vec3(float(tx), float(ty), 0) * vec3(texel_world_size, texel_world_size, 1)
         + vec3(0, 0, h * height_scale) - terrain_frame.camera_world_rel;
gl_MeshVerticesEXT[slot].gl_Position = terrain_frame.relative_vp * vec4(rel, 1.0);
```

- 中心差分采样次数：每顶点 5 次（中心 + 4 邻）；一组 81 顶点 ⇒ 405 次读。**先直白实现**，
  要优化时再让相邻线程经 `shared` 复用中心值。
- 高度读取里只做 `int` 索引与一次 `float()` 转换（整数域纪律，§4.6）。

### 4.4 Fragment

- 输入 `vUV`（格坐标）与 `vNormal`（mesh 阶段中心差分来的**光滑法线**，U4）。
- v2 第一版：常量色 × 简单兰伯特 + 法线可视化开关（`TERRAIN_DEBUG_NORMAL`）。
- 高程配色 / 材质 splatting 留 T4；那时若用 `Terrain` sampler 预设（Linear/Aniso）需注意 E7
  （mesh 阶段不能采样纹理，采样只能在片元阶段）。

### 4.5 边界与越界（v1 未覆盖，必须处理）

| 情形 | 处理 |
|---|---|
| 中心差分读到 tile 外 | **合法**（世界缓冲连续）；只有世界边界需处理 |
| 外扩格的采样落在邻居范围内 | **合法**（同上）；世界最外边界处 clamp ⇒ 外扩带退化为边界格的复制 |
| 世界边界处 `±step` 越界 | `texel = clamp(texel, 0, world_texels - 1)`（退化为一阶/零梯度，视觉可接受） |
| 世界尺寸不是 `kTileSpan` 整数倍 | **配置校验直接拒绝**（最后一个 tile 不满 8×8 格会破坏常量输出） |
| texel 世界尺寸随 LOD 变化 | 允许；但 LOD 级差必须是 2 的幂（`step_shift` 为整数） |

### 4.6 整数纪律

```
cell_id  = gl_WorkGroupID.x * 64 + gl_LocalInvocationIndex     // uint
texel    = tile.texel_origin + ((gx0 + vx) << step_shift)      // uint（位移，不是乘法）
index    = texel_y * stride + texel_x                          // uint ← 到此处全整数
h        = float(heights[index])                               // 最后才转 float
```

小整数（≤2²⁴）在 float 中精确 ⇒ 格坐标/texel 索引不怕 float；**怕的是世界坐标与「uv→世界」
的来回乘除** ⇒ 世界坐标只在 CPU 侧 double 累加，shader 内只出现局部/相对坐标。

---

## 5. LOD 与裂缝

### 5.1 误差度量与级差

```
proj_scale = viewport_height / (2 * tan(fov_y / 2))
target_px  = 4.0            // 一格在屏幕上不超过约 4 像素（可调）
step_shift = clamp(ceil(log2( 8 * texel_world_size * proj_scale / (target_px * distance) )), 0, max_shift)
若 (height_max - height_min) > 0.25 * kTileSpan * texel_world_size ⇒ step_shift += 1   // 起伏大则更细
```

**级差必须为 2 的幂**（`step_shift` 之差为整数），且 **tile 原点按 `kTileSpan`（2 的幂）对齐**（§2.1）
—— 两条合起来才保证「粗格点位置是细格点子集 ⇒ 同一 texel ⇒ 同一高度值」这一**顶点级重合**。

### 5.2 裂缝：本质、手段、判据（★ v1 结论已推翻）

**T 型交点缝是固有几何问题，不能靠采样方式消除。** 相邻 LOD 共享边上：细侧是折线（穿过多个顶点）、
粗侧是弦（只连两端点），二者只在「边上高度函数为线性」时重合。2 的幂步长 + 同一高度函数保证的是
**顶点级重合**，**不等于**边内部重合。双线性的作用是让裂缝**有界且可量化**，不是消裂手段。

| 手段 | 机制 | 代价 | 本方案 |
|---|---|---|---|
| **a. 外扩重叠（U6，采用）** | 每个 tile 的网格向 −X/−Y 各多生成 1 格；边界线两侧各有连续表面跨过 ⇒ 缝被**覆盖**（不是堵住） | 顶点 81→100、图元 128→162（上限 256 内）；**无第二套索引逻辑** | ✅ T2.3 |
| b. 向下裙边（RTF 原意） | 边缘额外向下挤一圈三角形，把缝"堵住" | 独立索引逻辑 + 绕序处理 + 裙深参数 + **需按裂缝深度标定** | ❌ 不采用（2026-09-28 指示） |
| c. 顶点形变 geomorph | 细侧过渡带顶点向粗侧弦插值 | 需 morph 因子 + 过渡带判定 | 可选优化，不进开发期 |

**外扩重叠的四条要点**

1. **ε 只需"避开共面"**：外扩带顶点高度 = 采样高度 − `overlap_epsilon × cell_world_size`（默认 `1e-3`）。
   ε **不是**裙深 —— 它不需要覆盖裂缝深度 ⇒ **不需要任何裂缝深度测量与标定**（相对裙边最大的稳定性收益）。
   唯一要求：ε 在该距离上大于深度精度（别比深度精度还小）。
2. **两侧对称外扩 ⇒ 不可能出现孔洞**：一条边界线既是"本 tile 网格的边缘"，又是"邻居网格的内部"
   （邻居已向本侧外扩 1 格）⇒ 缝总被至少一侧的**连续表面**跨过。
3. **平坦区无 z-fighting**：外扩带下沉 ε 后落在邻居表面之下（邻居自己那一格未下沉）；
   起伏区可能穿透邻居表面 ≤ LOD 误差 ⇒ 出现 1 格宽的细薄片（**不是孔洞**，视觉可接受）。
4. **外扩采样天然合法**：外扩格的 texel 落在邻居范围内 —— 高度缓冲是世界连续的（§2.3/§4.5），**不需要 halo**。

**判据（T2.3，可自动判定）**

1. **无孔洞**：固定相机截图 + 深度/覆盖率自检，边界带连续；
2. **无 z-fighting**：平坦区域（正弦环地形的高原/谷底）边界带连续多帧截图一致；
3. **对照（关键）**：`flags` 位0 关闭外扩 ⇒ 同相机应复现 T2.2 的可见裂缝；
4. **预算**：顶点 ≤ 100 / 图元 ≤ 162（管线创建即验证）。

第 1 条必须与第 3 条配对使用：**"开着没缝、关掉有缝"才算有效证据**；只报"开着没缝"无法区分
"确实遮住了"与"本来就没缝"。

---

## 6. 与引擎的接线面

### 6.1 新增文件

| 文件 | 内容 |
|---|---|
| `ShaderLibrary/mesh/terrain_grid.glsl.tmpl` | mesh 主体模板（照 `char_quad.glsl.tmpl` 结构） |
| `ShaderLibrary/vertex/s1_terrain_grid.glsl` | `TerrainHeightRef` / `TerrainTileRef` / `TerrainFrameRef` 的 `buffer_reference` 声明（照 `s1_text_char_quad.glsl`） |
| `src/ShaderGen/meshgen/MeshShaderModeTerrainGrid.h` | `EmitTerrainGridResources` + `EmitTerrainGridBody`（照 `MeshShaderModeCharQuad.h`） |
| （无新 GLSL 文件） | **T3** 的 compute 生成器：可写成 `ShaderLibrary/compute/terrain_cull.comp.glsl`（不经 meshgen，compute 路线无需新增 mesh 模式触点） |
| `inc/hgl/ecs/support/terrain/{TerrainBuildSystem,TerrainCollectSystem,TerrainRenderPipeline,TerrainRenderSystem}.h` + `src/.../*.cpp` | 专用管线四件套（照 `ecs/support/line/*`） |
| `inc/hgl/graph/ssbo/TerrainRows.h` | §2.1/§2.2 的结构 + `static_assert` |
| `ShaderLibrary/material/terrain.material.toml` | 材质入口（`[mesh_shader] mode = "TerrainGrid"`） |
| `ShaderLibrary/material/terrain_source.glsl` | 片元材质源（高程配色） |
| `example/Terrain/TerrainBasic.cpp` + `CMakeLists.txt` | 示例（T1 起逐个加到 T4） |

### 6.2 改动触点（9 处，缺一不可）

| # | 文件 | 改动 |
|---|---|---|
| 1 | `inc/hgl/mtl/MeshShaderMode.h` | 加 `TerrainGrid` 枚举 |
| 2 | 同上 | `ParseMeshShaderMode` 加分支（拼错必须显式失败，不允许静默降级） |
| 3 | 同上 | `GetMeshModeVerticesPerInvocation` / `PrimitivesPerInvocation`：地形是**组级固定产量**，需新增组级路径（§6.2.1） |
| 4 | `src/ShaderGen/meshgen/MeshModeDescriptor.h` | 新增 `ResolveTerrainGridTopology`（**组级**：8×8 / 9×8 / 9×9 格 ⇒ ≤100 顶点 / ≤162 图元，与 `max_invocations` 无关）+ 注册表第 4 项（defines 用标准发射；custom resources = 三个 `buffer_reference`；stage1/2/3 = `nullptr`） |
| 5 | `src/ShaderGen/builder/GenericMaterialBuilder.cpp` | 模式决策链加 `TerrainGrid` 分支（容量钳制：≤100/≤162 对 256 下限的余量校验） |
| 6 | `src/ShaderGen/material_definition/MaterialDefinitionFile.cpp` | `[mesh_shader] mode` 已知键允许 `TerrainGrid`（拼错必须显式失败） |
| 7 | `src/ShaderGen/builder/VertexABIBuilder.cpp` | 「无外部顶点输入」分支（与 CharQuad 同类） |
| 8 | `inc/hgl/graph/ShaderBufferSources.h`（X 列表 `:267` + `PushRootAddresses`） | ① `HGL_ROOT_ADDRESSES_FIELD_LIST` 追加 5 行（3×uint64 地址 + `terrain_tile_index` uint32 + `_pad_terrain` uint32 ⇒ 112B）；② `PushRootAddresses` 末尾追加 `terrain_tile_index = 0`。**GLSL 侧不用改**：`pc_root` 块由生成器遍历**同一** X 列表发射（`MeshShaderHeaderGen.h:28`、`MaterialShaderEmitter.cpp:510`），布局断言也按同一列表自动推导（`ShaderBufferSources.h:316/322`）⇒ 不再有"C++ 与 GLSL 同批改"这条风险 |
| 9 | 管线创建路径（`src/Vulkan/*`）+ `src/ecs/core/DefaultSystems.cpp` | 地形 mesh 管线注册；**T3** 增加独立 compute 管线（命令/表生成），mesh 管线本身不变 |

#### 6.2.1 触点 3/4 为什么与 CharQuad 不同（本版新发现）

CharQuad/LineQuad 的产能是「线程数 × 每线程 4 顶点 / 2 图元」，而地形是
**「每组固定产量、与线程数无关」**（顶点复用 + 外扩带；产量由该组在 tile 内的位置决定：≤100 顶点 / ≤162 图元）。
⇒ 容量计算与 `max_invocations` 无关；`GetMeshModeVerticesPerInvocation` 这类「每线程产量」接口对地形要
**显式回避**（新增组级接口，或让地形分支直接写常量），否则会算出一个错误的 `max_vertices`。

> 这套 per-threadgroup 语义与 `SetMeshOutputsEXT` 的塌方记录见
> `doc/mesh-shader-thread-model-and-loadvertexdata-conflicts.md` §2/§4（**第 0 条纪律**就是本条：组级产量必须是
> 编译期常量、槽位用线性分摊），本方案 §4.1 的格分摊写法即照它执行。
这一条必须在触点设计时就定，不能等编译报错再改。

### 6.3 材质 TOML（照 `text_2d_gpu.material.toml`）

```toml
schema = 3
id = "Terrain"
name = "Terrain"
provider_policy = "GeometryOnly"

[mesh_shader]
mode = "TerrainGrid"
max_invocations = 64          # 注：地形是组级固定产量（≤100/≤162），此键只用于线程数

[transform]
source = "None"                 # 无外部顶点输入（同 CharQuad）
mapping = "Passthrough3D"
orientation = "World"
scale = "World"
projection = "WorldCameraVP"    # 位置已在 shader 里算好

[fragment]
material_source_module = "material/terrain_source.glsl"
ntb_module = "ntb/ntb_derivative_normalmap.glsl"   # 占位；法线改由 mesh 阶段提供
```

---

## 7. 分阶段实施

每阶段收尾动作固定：**编译（引擎生成器 + 门）→ 运行（validation 零 error，单独 grep `result -4`）
→ 截图/自检 → tag**。

### 开发期原则（T1–T2.4 通用）

**以开发稳定性为准，不追求性能。** 具体到取舍：

| 原则 | 具体做法 |
|---|---|
| 一次只引入一个新机制 | 每步只加一样东西（多 tile → LOD → 外扩遮缝 → ICB），不加"顺便优化" |
| 每步都与上一步可逐像素对比 | 固定相机与 LOD 策略，`--screenshot` 存档；出现差异即可二分定位 |
| 参数传递走"看得见"的通道 | 每 tile 一次 draw、每次 draw 只有 4B 参数变化（`pc_root.terrain_tile_index`）；**不引入间接/`gl_DrawID`/前缀和反查/GPU 侧索引** |
| 不设性能目标 | draw 数 = tile 数、每帧重建 tile 表、单线程 CPU 合成——都接受；性能只在 T2.4 之后记录基线 |
| 每步一个可判定判据 | 见每步"判据"，且判据必须能自动跑（自检/统计/截图比对），不靠"看着像对" |

### T1 — 单 tile 直发（最小闭环）

- **内容**：`TerrainGrid` 模式 + 世界高度缓冲（uint16，程序生成的径向正弦环，照源工程语义）+
  `TerrainFrameData`（relative_vp 由 CPU 从引擎相机算）+ 单个 tile 的 `vkCmdDrawMeshTasksEXT`。
  只发 1 个 mesh workgroup（`groups_per_side = 1`、`step_shift = 0`），无 LOD、无外扩。
- **判据**：
  1. 出图、网格无空洞；validation 零 error（含 sync validation）；`grep "result -4"` 为空；
  2. 抽样 10 个 `(gx,gy)`：mesh 阶段输出的高度与 CPU 同索引值**逐位一致**；
  3. 法线：调试开关下图法线连续、无条带。
- **回滚**：tag `terrain-t1`；本阶段只新增文件 + 触点 1–8。

> **T2 = 四步**（用户 2026-09-28 指定）：1 多 tile → 2 LOD → 3 外扩遮缝 → 4 ICB 化。
> 前三步**都是直发**（每 tile 一次 `vkCmdDrawMeshTasksEXT` + 每 draw 一次 push），第四步才换提交机制。

### T2.1 — 多 tile 渲染（无 LOD、无外扩）

- **内容**：CPU 合成 tile 表（`BuildTileList(view, policy)` **纯函数**，便于 T3 原样搬进 CS）；
  **均质网格**：所有 tile `step_shift = 0`、`groups_per_side = kTileSpan / 8`（默认 32）；
  `kTileSpan` 与世界尺寸做整除校验（§4.5）。
  提交：`for (i = 0; i < tile_count; ++i) { push(pc_root, …, terrain_tile_index = i);
  cmd->DrawMeshTasks(groups_per_side², 1, 1); }`
  **tile 索引经 `pc_root.terrain_tile_index`**（§3.1）——直发期唯一能传 per-draw 标量的通道。
- **判据**：
  1. 全部 tile 出图、拼合无重复/空洞（边界高亮材质）；draw 数 == tile 数；
  2. 相邻 tile 共享边同位置高度差 == 0（此时 `step_shift` 全 0，是恒等，作为 T2.2 的基线）；
  3. 无 LOD 断言：所有 tile 的 `step_shift`/`groups_per_side` 相同（统计日志）。
- **回滚**：tag `terrain-t21`；只加 tile 表 + 循环 + 一个 pc_root 字段。

### T2.2 — 带 LOD 的多 tile 渲染（仍不遮缝）

- **内容**：§5.1 的误差度量（CPU 在 `BuildTileList` 里算 `step_shift`）+ min/max 金字塔（§2.4）+
  `groups_per_side = kTileSpan / (8 << step_shift)`（tile 跨度不变式，§2.1）。
  提交方式**不变**（逐 tile 直发；`groupCountX = N²` 随 LOD 变小）。
- **判据**：
  1. **顶点级重合**：相邻粗细 tile 共享边上所有粗格点位置，用细网格插值高度与粗网格顶点高度差 **== 0**；
  2. **"能看见裂缝"是预期结果**：同相机同位置截图存档，作为 T2.3 的对照证据（此时不应宣称无缝）；
  3. LOD 生效证据：相机拉远后 tile 的 `step_shift` 上升、总组数下降（统计日志断言趋势）。
- **回滚**：tag `terrain-t22`。

### T2.3 — 外扩重叠遮缝（取代向下裙边，U6）

- **内容**：§4.1 的网格格域扩到 `8N+1` 格/边（向 −X/−Y 各外扩 1 格），外扩带高度 −`overlap_epsilon × cell_world_size`；
  `TerrainTile.flags` 位0 可关闭外扩（调试/对照）。**不做向下几何、不引入裙深参数、不需要裂缝标定**。
- **判据**：见 §5.2 四条 —— 边界带无孔洞 / 平坦区多帧截图无闪烁 / **关掉外扩能复现 T2.2 的裂缝** /
  预算 ≤100 顶点、≤162 图元。
- **回滚**：tag `terrain-t23`；本步只改网格循环的格域与一个 ε，**提交与 LOD 逻辑一行不动**。

### T2.4 — ICB 化（CPU 生成 Indirect Command Buffer）

- **内容**：CPU 把命令写进 `IndirectMeshTaskBuffer`（`CreateIndirectMeshTaskBuffer` + 逐条 `WriteCmd`，
  每条 `{N*N, 1, 1}`）+ count 缓冲写本帧 tile 数；提交改为**一次**
  `cmd->DrawMeshTasksIndirectCount(mesh_tasks->GetVkBuffer(), 0, count_buffer->GetBuffer(), 0,
  max_draw_count, sizeof(VkDrawMeshTasksIndirectCommandEXT))`；
  mesh 里 `tile = terrain_tiles.t[gl_DrawID]`（**tile 索引不再经 push**；
  `pc_root.terrain_tile_index` 留作调试）。
  **仍不做剔除**（U1）——"tile 表里的每个 tile 都画"是本阶段要保的不变量。
- **收益（可测）**：draw 数 tile_count → **1**；CPU 提交代码在 T3 迁 CS 时**一字不改**；
  `gl_DrawID` 成为 tile 的唯一索引通道（引擎既有约定：命令序 = 表行序）。
- **判据**：
  1. 与 T2.3 **逐像素一致**；命令数 == tile 数 == 实际 mesh workgroup 数（pipeline statistics）；
  2. `BufferAllocPolicy::Auto` 的两条分配路径都验（本机有 ReBAR 走 `CPUVisible`，否则 `StagedUpload`；
     写后 `Flush` / staged 提交必须正确）；
  3. tile 表与命令缓冲每帧重建但按帧槽自洽（连续 ≥8 帧无异常）。
- **回滚**：tag `terrain-t24`；T2.1–T2.3 的直发路径保留为对照开关（`--direct-draw`）。

### T3 — 生成迁进 ComputeShader（GPU-driven，U1）

- **内容**：新增 compute pass：读相机与 tile 全表 → 写 tile 表（LOD/`groups_per_side`）+ 间接命令 +
  `atomicAdd` 写 count。CPU 退化为「填相机参数 + 提交一次间接绘制 + 每帧把 count 归零」。
  **本阶段仍不做剔除**（只把生成搬家，保证与 T2.4 行为等价）；屏障与帧槽规则一次做对。
- **判据**：
  1. 与 T2.4 **逐像素一致**；CPU 侧**零逐 tile 写入**（不再 `WriteCmd`）；
  2. 命令数 == tile 数（pipeline statistics）；CPU 侧绘制调用数 = **1** 且与 tile 数无关；
  3. count 归零 → CS 写 → 消费 的批次顺序正确，且**归零目标与上一帧消费不同帧槽**（§3.2/R8/R13）；
  4. 屏障：`COMPUTE_SHADER`/`SHADER_WRITE` → `DRAW_INDIRECT`/`INDIRECT_COMMAND_READ`（sync2 写法）；
     连续 ≥8 帧无异常、`grep "result -4"` 为空。
- **回滚**：tag `terrain-t3g`；T3 全是加法（T2.4 的 CPU 生成路径保留为对照开关）。

### T4 — 后续（列出以免架构走死）

- **CS 内 Frustum 剔除**（U1 的终态）：tile AABB = `texel_origin + kTileSpan + height_min/max`；
  剔除掉的 tile 不发命令（count 少一条）——**这是 T3 之后第一个真正减少工作量的阶段**，
  也是 §2.4 金字塔的主要用武之地；配套加地平线/背向剔除；
- 页流式 + 驻留表 + 非驻留降级（那时页需要 halo = 最大 step）；
- 高程配色 / 材质 splatting（用 `Terrain` sampler 预设，片元阶段采样）；
- 相机相对全量接入（等 `TransformAssignmentBuffer::SetCameraOffset` 完成）；
- 高度生成进 compute（正弦环/噪声/从 `HeightmapCompress` 工具产出的资产）；
- CS 内 LOD 决策（把 §5.1 从 CPU 搬进 CS，与剔除合并成一次 pass）；
- 性能基线记录（T1→T4 的 mesh 阶段耗时 / 三角数 / draw 数 / 组数，timestamp query，本机 52.08 ns）；
- 可选对照：Task Shader 分级分发（仅作开销对照评估，不做主线）。

---

## 8. 验证矩阵与执行纪律

| 项 | 手段 | 判据 |
|---|---|---|
| GLSL / 生成器 | 引擎生成器 + `ShaderResourceSchemaRegressionGate` | 门 **39 PASS / 0 FAIL** 不退化；产物内容断言（`grep -c` 关键宏） |
| 地址表结构 | 门 `S.global-addresses-struct-parity` | PASS（C++ 表 ↔ GLSL `GlobalAddressesRef` 同序同型）；**本方案不动该表**（地形四字段走 `pc_root`，§3.1/§3.3） |
| 对齐与尺寸 | 取址后断言 + 配置校验 + `spirv-val` | 高度基址 ≥2B、tile 表基址 ≥4B；世界 texel 尺寸整除 `kTileSpan`（**不要求 POT**）；`storageBuffer16BitAccess` 门为 true |
| 材质未被静默回退 | 渲染型门（ATS / CSM 那类） | 画面契约门非 0 px 变化 |
| 设备丢失 | 每个渲染门单独 | `grep "result -4"` 为空；无 `addr_*=0` 警告 |
| 高度语义 | `TerrainBasic` | 抽样 10 点与 CPU 同索引值逐位一致 |
| 拓扑预算 | 管线创建 + 捕获 | 顶点 ≤ 100 / 图元 ≤ 162（角组）；`SetMeshOutputsEXT` 组内一致；无越界写 |
| tile 拼接 | `TerrainTiles` | 无重复/空洞 |
| LOD 接缝 | `TerrainLod` | 共享边粗格点高度差 == 0；未遮缝时**预期可见裂缝**（截图存档作对照） |
| 帧槽正确性 | `TerrainLod` / `TerrainGpuDriven` | 连续 ≥8 帧（跨主帧槽与离屏槽）无「整帧清屏色」 |
| 多 tile（T2.1） | `TerrainTiles` | 全部 tile 出图、拼合无重复/空洞；draw 数 == tile 数；所有 tile 同 `step_shift` |
| LOD（T2.2） | `TerrainLod` | 共享边粗格点高度差 == 0；**预期可见裂缝**（存档作对照）；距远 tile 的 `step_shift` 上升 |
| 外扩遮缝（T2.3） | `TerrainSkirt` | 边界带无孔洞；平坦区多帧截图无闪烁；**关闭外扩能复现裂缝**；顶点 ≤100 / 图元 ≤162 |
| ICB 提交（T2.4） | `TerrainIndirect` | 命令数 == tile 数 == workgroup 数；与 T2.3 逐像素一致；`Auto` 两条分配路径都验 |
| CS 生成（T3） | `TerrainGpuDriven` | 与 T2.4 逐像素一致；CPU 零逐 tile 写入；绘制调用数 = 1；count 归零与帧槽纪律 |
| CS 剔除（T4） | 同上 | 视野外 tile 的命令**不被写入**（count 变小）；可见集与 CPU 参考实现逐 tile 一致 |
| 性能 | timestamp query（52.08 ns） | 记录 T1→T4 的 mesh 阶段耗时 / 三角数 / workgroup 数 |
| 回归 | `SimpleMeshTriangle`、`TextDrawTest`、`LineRenderTest`、`CascadeShadowMap` | 全部照常出图；`PushRootAddresses` 调用点语义不变 |

---

## 9. 风险与开放问题

| # | 风险 | 默认选择 |
|---|---|---|
| **R1** | `PushRootAddresses` 是 12 位置参数的裸签名（S1 曾把 `addr_global_addresses` **插到第 4 位**，风险应验过一次） | 本方案只在**末尾追加带默认值的参数**（`terrain_tile_index = 0`）⇒ 现有调用点零改动；**绝不再往中间插**。若将来要加第二个，先结构体化 |
| R2 | mesh 输出是**四重预算**（vertices / primitives / memory / components） | 按**规范下限 256** 设计（≤100/≤162 有余量）；管线创建即失败便于早发现 |
| R3 | 触点 3/4 的「组级产能」与现有「每线程产量」接口语义冲突（§6.2.1） | 显式新增组级接口；**不要**用 `max_invocations × 每线程产量` 去凑 |
| R4 | `groupCountX = N²` 展平后受**单维上限 65535** 限制（本机实测 `maxMeshWorkGroupCount = {65535,65535,65535}`） | 约束 `N ≤ 256`（本方案 N ≤ 32）；配置校验里断言 `N*N ≤ 65535` |
| R5 | 外扩带与邻居表面共面 ⇒ z-fighting（平坦地形最明显） | 外扩带 −ε + **两侧对称外扩**（§5.2）；判据里含"平坦区多帧截图一致" |
| R6 | LOD 跳级（级差 > 1） | §5.1 对任意 2 的幂级差成立；T3 自检覆盖跳级用例 |
| R7 | 相机相对尚未全量接入 | 自算 relative_vp（§3.4）；T4 再收敛到引擎机制 |
| R8 | 帧槽别名（ring 深度 < 8） | 按 `HGL_FRAME_SLOT_TOTAL` 分配；连续 8 帧自检（§8） |
| R9 | 高度界金字塔的构建位置 | 先 CPU（可自检）；T4 的 CS 剔除直接消费它，届时可一并搬进 CS |
| R10 | 大世界 texel 数超出 tile 表上界 | `kTileSpan` 与世界尺寸做配置校验（§4.5）；容量 = 世界尺寸 / tile 尺寸 |
| R11 | 源工程「高度不能独立缩放」的缺陷 | 本方案高度与 XY 分离（`height_scale` 只作用于 Z），天然不存在 |
| R12 | 残留 exe / 旧二进制导致假回归 | §3.3 第 3 条（taskkill 单斜杠、跑前核验、见 `MSB8066` 即作废本轮结果） |
| R13 | T3 起 count 归零写坏在途帧的命令/表（CS 本帧写、上一帧还在消费） | 命令/表/count 三样都按 `HGL_FRAME_SLOT_TOTAL` ring 分份；归零与消费不同槽；连续 8 帧自检 |
| R14 | `gl_DrawID` 依赖 `shaderDrawParameters`（本机 true） | 启动断言该 feature；缺失时退化为「每 tile 一次 `vkCmdDrawMeshTasksIndirect` + per-draw push」，仅在受限设备上 |
| R15 | 命令缓冲分配路径差异（ReBAR 走 `CPUVisible`、否则 `StagedUpload`） | 写入后按路径 `Flush`/staged 提交；把「两条路径都跑一遍」写进 T2 判据 |
| R16 | 外扩带在起伏处穿透邻居表面 ⇒ 1 格宽的细薄片 | **不是孔洞**（宽度 = 1 格），接受；ε 不宜取大（过大会在邻居表面下露出边） |
| R17 | 外扩带来每边 1 格的重叠绘制（overdraw） | 开发期接受（稳定性优先）；若日后成瓶颈，可只让"较粗一侧"外扩（需邻接 LOD 信息） |
| R18 | 物理指针访问的 `Aligned` 不满足（16 位 ⇒ 2、tile 行 ⇒ 4）⇒ validation error / 设备丢失；多条表拼进一个 buffer 时偏移未取整 | 取址后断言对齐；拼段时各段偏移向上取整到 `max(16, 类型对齐)`（本机 `minStorageBufferOffsetAlignment = 64`）；详见 §2.6 |
| R19 | 16 位读取写成 `float(h)` 被 glslc 拒绝（`'constructor' : can't convert`） | 一律 `float(uint(h))` 两级转换（§2.6 已实测） |

---

## 10. 变更记录（v1 → v2）

| # | v1 的说法 | v2 的结论 | 原因 |
|---|---|---|---|
| 1 | 「2 的幂步长 + 双线性采样即可无缝，**不需要裙边**」 | **错**：那是顶点级重合，边内部仍是 T 型缝；**必须做遮缝**（本版用外扩重叠，见 #18） | 细侧折线 vs 粗侧弦；双线性的作用是让裂缝有界可量化，不是消裂 |
| 2 | 每线程 1 格 = 4 顶点（8×8 格 = 256 顶点） | **9×9 = 81 顶点**（顶点复用 −68%；外扩后最坏 10×10 = 100） | 采纳 `Terrain Vulkan 1.4.rtf`（U2） |
| 3 | push constant 塞 `mat4 vp`（104B）、扩展 `RootAddresses` 加地形表地址 | `pc_root` **不动**；三个地址进全局地址表（§3.1），PC 里不放矩阵 | 基线已变（E3/E4/E5）：相机走 BDA、表按帧槽分份；且 U7 要求精简 PC |
| 4 | 「在途帧资源单份、引擎不代管」（限制三） | 引擎已有**按帧槽分份**机制（E5/E6），照 L2W / CameraInfo / shadow 的 ring 做 | 基线更新 |
| 5 | 材质 TOML 可声明 `ubos = [...]` | **硬不变量：声明的描述符必须为空**（E2） | Scene 集退场 |
| 6 | 高度 `uint32` 装 16 位值 | **原生 `uint16_t[]`**（U3） | 采纳 RTF；本机 feature 支持 |
| 7 | 片元 `dFdx/dFdy` 法线 | **mesh 阶段中心差分**（U4） | 采纳 RTF |
| 8 | 用引擎相机矩阵直接变换 | 自算 **relative_vp**（§3.4） | 引擎 camera-relative 未启用；U5 要求相机相对 |
| 9 | 页 + tile 两级（v1 正文） | **只做 tile**，页降级为字段（§2.5） | 简化；流式不是 v2 目标 |
| 10 | 边缘组 `SetMeshOutputsEXT` 全组一致 + 紧凑槽位（v1「头号陷阱」） | 固定 8×8 格拓扑下**不存在该陷阱**（原为常量 117/192；外扩后为 ≤100/≤162，仍是每组的运行期常量、组内一致）；另加「世界尺寸必须整除 `kTileSpan`」 | U2 的顶点复用顺带消掉它 |
| 11 | 无（v1 未覆盖） | 新增：中心差分在 tile 边界的合法越界读、世界边界 clamp、tile 跨度 2 的幂对齐（§4.5/§5.1） | 实现前必须定的细节 |
| 12 | 无（v1 未覆盖） | 新增：**组级产能**接口问题（§6.2.1）、间接提交形态（D3） | 开工前必须定的接口 |
| 13 | Task Shader 原生级联分发（采纳 RTF 的 U1） | **改为 ComputeShader + MeshShader**；先 CPU 生成 Indirect Command Buffer 且**不做剔除**，将来迁进 CS 并在 CS 里做 Frustum 剔除 | 2026-09-28 用户指示（Task Shader 路线取消） |
| 14 | 无（v1 未覆盖） | 新增：`groupCountX = N²` 的单维上限 65535、命令缓冲两条分配路径、`gl_DrawID` 的 feature 依赖 | 实现前必须知道的约束 |
| 15 | T2 = 「多 tile + CPU 生成 ICB」一步 | **拆成四步**：T2.1 多 tile → T2.2 LOD → T2.3 外扩遮缝 → T2.4 ICB 化；原 T3（LOD 与裂缝）被 T2.2/T2.3 吸收，原 T4/T5 顺延为 T3/T4 | 用户 2026-09-28 指示（每步只引入一个新机制） |
| 16 | 地址落位推荐 A（全局地址表） | **推荐 B（`pc_root` 四字段）**：直发期必须传 per-draw 的 tile 索引，而 GLSL 每 stage 只有一个 `push_constant` 块 ⇒ 只能进 `pc_root`；四字段一起走它机制最少。T2.4 后可迁回 A（可选） | 用户 2026-09-28 指示（开发期稳定优先）+ 4 步拆分的连带影响 |
| 17 | 无（v1 未覆盖） | 新增「开发期原则」表：一次只引入一个新机制、每步可逐像素对比、参数走看得见的通道、不设性能目标、每步一个可自动判定的判据 | 用户 2026-09-28 指示（以开发稳定性为准） |
| 18 | 裙边 = 向下挤出（同组产 36 顶点 / 64 图元，合计 117/192） | **改为「与邻居外扩重叠」**：网格格域 = `8N+1` 格/边，外扩带 −ε；预算 → **≤100 顶点 / ≤162 图元**；**无第二套索引逻辑与绕序处理** | 用户 2026-09-28 指示（类似 Unreal 的重叠格式，以简化网格生成；源工程 `splitHeightFieldIntoBlocks` 的"1 行/列重叠分块"即此族祖先） |
| 19 | 需要按裂缝深度标定 `skirt_depth`（判据「实测裂缝深度 ≤ skirt_depth」） | **整条删除**：ε 只需"避开与邻居共面"、不需覆盖裂缝深度 ⇒ 无标定；判据改为「无孔洞 + 平坦区无 z-fighting + **关掉能复现裂缝**」 | 外扩重叠的机制性质（覆盖，而非堵塞） |
| 20 | 无（v1 未覆盖） | 新增 **§2.6 对齐与尺寸约束**：明确「走 buffer 不是纹理 ⇒ 纹理对齐规则全不适用」；**2 的幂加在格步长与 tile 跨度上，不加在高度图尺寸上**（世界尺寸只需整除 `kTileSpan`）；给出三条 BDA 对齐 VUID、容量天花板（≈4 GiB / 边长 ≤46340 texel）、16 位路径实测（`Aligned 2`/`Aligned 4`、`float(uint(h))` 两级转换） | 用户 2026-09-28 提问「超大纹理要对齐吧？高度图不需要 2 次幂吗」 |

---

## 附录 A：关键文件索引

| 类别 | 路径 |
|---|---|
| 地址表真源 | `inc/hgl/graph/ubo/GlobalAddresses.h`、`ShaderLibrary/ubo/scene_ubo.glsl` |
| push constant | `inc/hgl/graph/ShaderBufferSources.h:267-278`、`inc/hgl/graph/RootAddressPush.h:31-43` |
| mesh 模式注册 | `inc/hgl/mtl/MeshShaderMode.h`、`src/ShaderGen/meshgen/MeshModeDescriptor.h`、`MeshShaderModeCharQuad.h` |
| mesh 模板 | `ShaderLibrary/mesh/{char_quad,line_quad,vertex_passthrough}.glsl.tmpl` |
| 专用管线先例 | `inc/hgl/ecs/support/line/*`、`src/ecs/support/line/LineRenderPipeline.cpp`（`:260-286` 绘制、`:772` push 根地址） |
| 间接命令缓冲 | `inc/hgl/vk/buffer/IndirectCommandBuffer.h`、`src/Vulkan/buffer/IndirectCommandBuffer.cpp:88`（`CreateIndirectMeshTaskBuffer`） |
| 间接提交 | `src/Vulkan/VKCommandBufferRender.cpp:443-472`、`src/ecs/support/PipelineMaterialRenderer.cpp:39-65`（`gl_DrawID` 用法） |
| compute 间接示例 | `example/Basic/ComputeIndirectCount.cpp`、`GPUIndirectCountHookSystem` |
| 设备能力 | `VP_VULKANINFO_Intel(R)_Arc(TM)_140T_GPU_(16GB)_101_8991.json` |
| 门 | `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp` |
| 帧槽 | `inc/hgl/common/RenderOptions.h:16-33`、`inc/hgl/graph/core/GraphicsContext.h:130-131` |
| 基线与纪律 | `doc/global-addresses-bda-unification-plan.md`、`doc/backlog.md` |

## 附录 B：术语

| 术语 | 含义 |
|---|---|
| tile | 绘制/决策单位：世界尺寸恒定的正方形（默认 `kTileSpan = 256` texel） |
| 格（cell） | tile 内的一个四边形；`step_shift` 决定它的 texel 跨度 |
| 组（workgroup） | 64 线程，覆盖 8 格（每轴最后一组 9 格），产 ≤100 顶点 / ≤162 图元 |
| `groups_per_side` | tile 每边发多少个组（= LOD 的体现） |
| 外扩重叠带 | 每个 tile 向 −X/−Y 各多生成的 1 格网格，与邻居几何重叠以覆盖 LOD 接缝（取代向下裙边，U6） |
| 高度界金字塔 | 每层 tile 的 min/max 高度树（**不是** mipmap） |
| 帧槽 | `HGL_FRAME_SLOT_TOTAL=8` 份 per-frame 存储；主帧 `[0,4)`、离屏 `[4,8)` |
| BDA | `VkDeviceAddress` / GLSL `buffer_reference` |
| 直发期 | T2.1–T2.3：每 tile 一次 `vkCmdDrawMeshTasksEXT`，tile 号经 `pc_root.terrain_tile_index` |
| ICB 化 | T2.4：CPU 填 `IndirectMeshTaskBuffer` + 一次 `DrawMeshTasksIndirectCount`，`gl_DrawID` 取代 per-tile push |

## 附录 C：公式速查

```
tile_span_texels = 8 * groups_per_side * (1 << step_shift) = kTileSpan        // 不变式
texel            = tile.texel_origin + ((group_id * 8 + local_xy) << step_shift)
index            = texel_y * stride + texel_x                                 // 到此处全整数
h                = float(heights[index]) * height_scale                       // 最后才转 float
world_rel        = vec3(texel) * texel_world_size + z * (h * height_scale) - camera_world_rel
clip             = relative_vp * vec4(world_rel, 1)
step_shift       = clamp(ceil(log2(8 * texel_world_size * proj_scale / (target_px * distance))), 0, max)
tile 自身面积    = 8N × 8N 格；网格格域 = 8N+1 格/边（外扩 1 格）；顶点域 = 8N+2/边
BDA 对齐         : uint16_t[] ⇒ Aligned 2；tile 行结构 ⇒ Aligned 4（VUID-06314/06315）
容量天花板       : 单缓冲/单次分配 ≈ 4 GiB ⇒ 16 位高度世界边长 ≤ 46340 texel（§2.6）
世界尺寸约束     : world_texels % kTileSpan == 0（**不要求 2 的幂**）
overlap_epsilon：外扩带下沉量 = overlap_epsilon * cell_world_size（默认 1e-3；只需避开共面，无需标定）
```
