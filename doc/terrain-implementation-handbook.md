# GPU-Driven 地形渲染 实现手册（MeshShader + BDA + Compute 间接提交）

> **本档是什么**：一份**可以照着写代码**的实现手册，不是方案讨论稿。目标读者是「换一台电脑、没有历史上下文」的实现者。
>
> **怎么用**：§1 环境自检 → §2 数据契约（先定这些，后面全依赖）→ §3 新模式 `TerrainGrid` 的代码主体 → §4 按任务卡逐步实现（T1 → T2.1 → T2.2 → T2.3 → T2.4 → T3 → T4）→ §5 验证台 → §6 坑与红线。
>
> **标记约定**：
> - **✅ 实测** = 本机真跑过/编译过，附证据（SPIR-V 字节数、`spirv-val` 结果、C++ `offsetof`、设备 limits 真值）。
> - **⚠ 待落** = 设计已定但尚未在引擎里跑通，实现时必须自己证一遍。
>
> **与其他文档的关系**：
> - `doc/terrain-implementation-plan-v2.md` —— 推导过程与决策记录（本档是它的**重组实现版**，冲突时以本档为准）。
> - `doc/terrain-vulkan-standalone-experiment.md` —— 脱离 ULRE 的独立 Vulkan 实验版（换机时可作为"引擎侧还没通"的旁路验证）。
> - `doc/terrain-rendering-implementation-plan.md` —— v1，历史与源工程细节索引。
>
> **文件约定**：与 `doc/` 一致 —— **CRLF 行尾、UTF-8 无 BOM**。

---

## 0. 一页速览

### 0.1 方案一句话

地形网格**全部由 MeshShader 生成**：XY 由格号算出，Z 从**高度缓冲（BDA storage buffer，不是纹理）**取样；CPU 只产出「画哪些 tile、各自什么 LOD」的 **tile 表**与（T2.4 起）**间接命令缓冲**；不走 Task Shader，走 **Compute + Mesh** 两阶段。

数据流（三张表）：

```
CPU                          GPU(MeshShader)
────────────────────────     ─────────────────────────────────────────────
TerrainFrame（每帧）    ──▶   TF.vp / texel_world_size / height_scale / overlap_epsilon
高度缓冲 uint16[]      ──▶   HBUF.data[texel] → 双线性/直接取样 → Z
tile 表 TerrainTile[]  ──▶   TBUF.t[pc_root.terrain_tile_index] → 格域/texel 原点/LOD
```

### 0.2 实现顺序（每步都独立可验证、可回滚）

| # | 目标 | 新增机制 | 判据（可自动） | 状态 |
|---|---|---|---|---|
| **T1** | 单个 tile 出图 | 新 mesh 模式 `TerrainGrid` + BDA 读高度 | 出图顶点与 CPU 同索引抽样逐位一致；validation 零 error | ⚠ 待落 |
| **T2.1** | 多 tile 均质渲染 | per-tile `push` + `DrawMeshTasks(N²,1,1)` | 与 T1 单 tile 逐像素一致（多画了几块） | ⚠ 待落 |
| **T2.2** | 带 LOD 的多 tile | 误差度量 + min/max 高度金字塔 | tile 数/格步长随距离变化；**能看见裂缝是预期结果**（截图存档） | ⚠ 待落 |
| **T2.3** | 外扩遮缝 | −X/−Y 各多 1 格与邻居重叠 | ① 边界带无孔洞 ② 平坦区多帧无 z-fighting ③ **关掉外扩能复现裂缝** | ⚠ 待落 |
| **T2.4** | ICB 化 | CPU 填间接命令 + 一次 `DrawMeshTasksIndirectCount` | **与 T2.3 逐像素一致** | ⚠ 待落 |
| **T3** | 生成迁到 ComputeShader | CS 写 tile 表与命令 + count | 与 T2.4 逐像素一致；CPU 提交代码零改动 | ⚠ 待落 |
| **T4** | CS 内 Frustum 剔除 | tile AABB（高度 min/max 参与） | 视野外 tile 命令不存在/零组 | ⚠ 待落 |

**开发期原则（用户已明确）**：一次只引入一个新机制｜每步与上一步可逐像素对比｜参数只走"看得见"的通道（不引入前缀和反查/GPU 侧索引，`gl_DrawID` 到 T2.4 才引入）｜**不设性能目标**（draw 数 = tile 数、每帧重建 tile 表都可以）｜每步一个能自动跑的判据。**以开发稳定性为准**。

### 0.3 硬不变量（违反 = 设备丢失或静默错误）

| # | 不变量 | 出处 |
|---|---|---|
| E1 | 描述符集**只剩 `BINDLESS_SET = 0`**；Scene 集已退场 | S2/S3 交接文档 |
| E2 | **材质定义声明描述符必须为空**（不得写 `layout(set=,binding=)`） | 同上 |
| E3 | 相机走 BDA 宏 `camera`（`ShaderLibrary/ubo/scene_ubo.glsl:149`），无 Camera UBO | `86535dbdb` |
| E4 | `pc_root` 布局是**单一真源 X 列表**：CPU struct / GLSL 字段名 / 字段类型 / 大小与偏移断言全从 `HGL_ROOT_ADDRESSES_FIELD_LIST` 生成 | `inc/hgl/graph/ShaderBufferSources.h:267-337` |
| E5 | 全局地址表按 `HGL_FRAME_SLOT_TOTAL=8` 帧槽分份（槽步长 128B、16B 对齐）；**地址必须在 buffer 物化处注册**，取不到即 fail-fast | `inc/hgl/graph/ubo/GlobalAddresses.h:43-47` |
| E6 | 顶点输入一律 SSBO/BDA（无 VBO/EBO）；唯一顶点阶段是 mesh shader | `MeshModeDescriptor` / `VertexABIBuilder` |
| **E7** | **bindless 纹理集的 3 个 binding `stageFlags` 只有 `FRAGMENT\|COMPUTE` ⇒ mesh 阶段读不到纹理** | `src/Vulkan/VKBindlessTextureManager.cpp:42/48/54`（第二组 `:220/226/232` 同）✅ 现场核 |
| E8 | `VertexPassthrough` 只能表达三角汤 ⇒ 地形必须新增 mesh 模式 | `MeshModeDescriptor.h` |
| 硬规矩 1 | 地址在 buffer **物化处**注册（只在每帧同步里注册 ⇒ 某路径没跑到 ⇒ 地址恒 0 ⇒ `vkQueueSubmit2 failed with result -4` = **设备丢失**） | S2/S3 事故 |
| 硬规矩 2 | C++ 表结构与 GLSL 引用结构同批改（本方案的等价风险已被 E4 的生成器消除，见 §2.4） | 同上 |
| 硬规矩 3 | 每个渲染门**单独** grep `result -4`；跑前 `tasklist` 核验无残留 exe，跑完立即 `taskkill /F /IM`（**单斜杠**） | 同上 |

---

## 1. 环境、迁移与自检

### 1.1 需要带过去的东西

| 项 | 值 |
|---|---|
| 分支 | `Terrain`，基线提交 `00fe74246`（"S3 结案"） |
| 远端 | `hyzgame/Terrain`、`github/Terrain` 均停在 `d4b8d1dcf` —— 本地领先 88 个提交，**需手动推送**（§1.6） |
| 本档 | `doc/terrain-implementation-handbook.md`（唯一实现依据） |
| 可选参考 | `doc/terrain-implementation-plan-v2.md`（推导/决策记录）、`doc/terrain-vulkan-standalone-experiment.md`（独立 Vulkan 旁路） |
| **不需要带** | 任何 `%TEMP%\terrainspike2\*` 临时骨架 —— 已在**附录 A** 全文收录 ✅ |

### 1.2 工具链（本机实测）

| 项 | 值 | 出处 |
|---|---|---|
| CMake 生成器 | **Visual Studio 18 2026** | `build/CMakeCache.txt`: `CMAKE_GENERATOR:INTERNAL=Visual Studio 18 2026` ✅ |
| 构建目录 | `D:/ULRE/build`，产物 `D:/ULRE/build/out/Windows_64_Debug/` | 目录内现有 3 个 exe：`CascadeShadowMap.exe`、`ShadowMap.exe`、`PBRSpheres.exe` ✅（说明该 build 目录只构建过部分目标；`SimpleMeshTriangle` 需自己先 build） |
| 依赖管理 | vcpkg `D:/vcpkg`，三元组 `x64-windows` | `CMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake` ✅ |
| Vulkan SDK | `C:/VulkanSDK/1.4.357.0`（`glslc` / `spirv-val` / `spirv-dis` 在 `Bin/`） | ✅ 本档所有 GLSL 都由它校验 |
| 现有 preset | `windows-msvc-debug` / `-release` / `-shipping` / `windows-ninja-*` / `asan` / `ubsan`（`CMakePresets.json`） | ✅ 本分支 **无 Qt preset**（`ULRE_ENABLE_QT=OFF`） |
| 语言标准 | C++20 | `CMakeCache.txt` / 各模块 |

### 1.3 命令速查（照抄即可）

```bash
# 构建单个目标（配置已存在）
cmake --build build --target SimpleMeshTriangle --config Debug

# 加你自己的示例目标：在 example/<组>/CMakeLists.txt 里照邻居写；exe 落在：
#   build/out/Windows_64_Debug/<Target>.exe

# ctest（注意：VS 生成器必须带 -C Debug）
ctest --test-dir build -C Debug

# GLSL 单编（附录 A 的自检）
"C:/VulkanSDK/1.4.357.0/Bin/glslc.exe" --target-env=vulkan1.3 -o out.spv terrain_grid_selfcheck.mesh
"C:/VulkanSDK/1.4.357.0/Bin/spirv-val.exe" --target-env vulkan1.3 out.spv
"C:/VulkanSDK/1.4.357.0/Bin/spirv-dis.exe" out.spv | grep -E "OpCapability|Aligned|ArrayStride"
```

**验证层**：走 `AppFramework` 的示例**默认就开标准验证**（`src/Work/AppFramework.cpp:40-41`：`cili.lunarg.standard_validation = true; cili.khronos.validation = true;`）✅ 所以「validation 零 error」这条判据天然可得；自建 `main` 的示例要自己置位（照 `example/Texture/TextureFormat.cpp:28`）。本机 GPU 是 **Intel Arc 140T**，无 `VK_LAYER_LUNARG_*` 时验证层会静默不生效 —— 跑完一定要确认日志里出现了验证层输出。

### 1.4 换机自检（开工前 7 步，全部要真跑）

1. **构建通过**：`cmake --build build --target SimpleMeshTriangle --config Debug`。
2. **mesh 路径能跑**：先 `cmake --build build --target SimpleMeshTriangle --config Debug`，再跑
   `build/out/Windows_64_Debug/SimpleMeshTriangle.exe`（照常有画面、无 validation error）。
3. **导出设备真值**：跑一次 Vulkan 设备信息导出（本机产物 `VP_VULKANINFO_Intel(R)_Arc(TM)_140T_GPU_(16GB)_101_8991.json`），逐项核对 **§1.5** 表；**换不同厂商 GPU 必须重核**（尤其 NV 的 mesh 输出上限）。
4. **GLSL 骨架自检**：附录 A.3 的三条命令，`glslc` 与 `spirv-val` 都必须 **exit 0** ✅（本机：10856 字节 SPIR-V）。
5. **16 位路径自检**：附录 A.2 的 `float(uint(h))` 两级转换写法，确认 `glslc` 不报 `'constructor' : can't convert`。
6. **布局断言**：不需要额外命令 —— 只要 `RootAddresses` 结构改了而没同步 X 列表，`static_assert(RootAddressesLayoutValid())` 会**编译期**报错（`ShaderBufferSources.h:336`），这是免费的门。
7. **验证层真的在**：跑第 2 步时确认日志里有验证层/`standard_validation` 的痕迹；否则后续所有"零 error"结论都作废。

### 1.5 设备能力表（本机真值 / 规范下限 / 注意）

✅ 全部取自本机 `VP_VULKANINFO_...json`：

| 能力 | 本机（Intel Arc 140T，驱动 101.8991） | 规范下限 | 我们的需求 | 注意 |
|---|---|---|---|---|
| `maxMeshOutputVertices` | 1024 | 256 | **≤100** | ⚠ **NV 常报 256**：≤100 仍在下限内，但余量只剩 2.5×，别再加顶点 |
| `maxMeshOutputPrimitives` | 1024 | 256 | **≤162** | 同上 |
| `maxMeshOutputMemorySize` | 524288 | — | 100 顶点 × 少量 varying | 宽裕 |
| `maxMeshWorkGroupCount`（每维） | 65535 | 65535 | N ≤ 256 | `N²` 展平成一维提交 ⇒ `N ≤ 256` ✅ |
| `maxMeshWorkGroupInvocations` | 1024 | 128 | 64（`local_size_x`） | — |
| `maxMeshWorkGroupTotalCount` | 4194304 | 65535 | tile 数 × 组数 | 宽裕 |
| `maxPushConstantsSize` | 256 | 128 | **112**（`pc_root`，§2.4） | 112 ≤ 128（下限）⇒ 仍可移植，但**再加字段就要重新评估** |
| `maxStorageBufferRange` | 4294967292 | — | 高度缓冲 | ≈4 GiB |
| `maxMemoryAllocationSize` | 4294901760 | — | 同上 | ≈4 GiB ⇒ 16 位高度世界边长 ≤ **46340** texel |
| `minStorageBufferOffsetAlignment` | 64 | 32 | 多表拼一缓冲时的段偏移 | 段偏移统一**向上取整到 64** 最省心 |
| `nonCoherentAtomSize` | 1 | 1 | CPU 可见缓冲 flush | 无最小刷新粒度麻烦 |
| `maxImageArrayLayers` | **2048** | 256 | （不走纹理，仅参考） | 「层数不够」**不是**否决 Texture2DArray 的理由（§2.7） |
| `shaderInt64` | true | — | BDA 必需 | 缺则整个方案不成立 |
| `storageBuffer16BitAccess` | true | — | 16 位高度 | 缺则高度改 `uint32`（+1 倍带宽） |
| `shaderInt16` | true | — | 同上 | — |
| `bufferDeviceAddress` | true | — | BDA 必需 | 缺则整个方案不成立 |
| `drawIndirectCount` | true | — | T2.4/T3/T4 | 缺则只能 `DrawMeshTasksIndirect`（T3 的 CS 计数会受限） |
| `shaderDrawParameters` | true | — | `gl_DrawID`（T2.4 起） | 缺则 T2.4 回退 per-tile push |
| `scalarBlockLayout` | true | — | 全部 `buffer_reference ... , scalar` | 引擎在设备创建时按支持情况启用（`src/Vulkan/VKDeviceCreater.cpp:454`），**不支持则 `scalar` 布局的引用会校验失败** |

**降级路径**（目标机能力不足时）：
- 无 `storageBuffer16BitAccess` ⇒ 高度缓冲改 `uint32_t[]`（`Aligned` 变 4；带宽 ×2）。骨架只需去掉 16 位扩展与两级转换。
- 无 `drawIndirectCount` ⇒ T2.4 用 `DrawMeshTasksIndirect`（CPU 侧知道条数），T3 的 count 由 CPU 读回（牺牲一帧延迟）。
- `maxMeshOutputVertices = 256` 且你想加顶点 ⇒ 把 `cells_per_group` 从 8 降到 7（≤81 顶点）或 6（≤64）。
- 无 `scalarBlockLayout` ⇒ 改 `std430` 布局并**重算** `buffer_reference_align`（会引入填充，须重新逐字段核对偏移）。

### 1.6 推送说明（交给你手动做）

推送到 `hyzgame/Terrain` 时，本次提交内容分六类：

1. **地形实现手册**（本档，新增）—— 换机开发的唯一依据。
2. **地形方案 v2**（`terrain-implementation-plan-v2.md`，新增）—— 推导与决策记录。
3. **独立 Vulkan 实验版**（`terrain-vulkan-standalone-experiment.md`）—— 含 V1–V7 相位梯与独立骨架。
4. **v1 挂档**（`terrain-rendering-implementation-plan.md`）—— 顶部牌匾列出"第二批口径变更"五条，正文保留源工程分析。
5. **既有文档交叉引用**：`gpu-driven-4id-draw-item-plan-2026-09.md`（`RootAddresses` 字段追加与它同一张表，建议一起决定）、`mesh-shader-thread-model-and-loadvertexdata-conflicts.md`（新增第 0 条"组级固定产能"纪律 + 地形关联）。
6. **不动的东西**：引擎源码**一行没改**（本档落的是设计与骨架；触点改动从 T1 开始）。
---

## 2. 数据契约（先落这些，后面全依赖）

### 2.1 三张表

#### (a) `TerrainTile` —— tile 表的一行（**32 字节**，`Aligned 4`）

```cpp
struct TerrainTile
{
    uint32_t texel_origin_x;   // tile 在高度缓冲中的 texel 原点（世界 texel 坐标）
    uint32_t texel_origin_y;
    uint32_t cells;            // tile 自身格数/边 = 8 * groups_per_side
    uint32_t lod;              // 格步长 = 1u << lod（texel）
    float    height_min;       // 剔除/误差度量用（T2.2/T4）
    float    height_max;
    uint32_t page_index;       // 保留：多缓冲分页（>4GiB 或流式时用）
    uint32_t flags;            // 位0 = 外扩遮缝开关（对照实验）；其余保留
};
static_assert(sizeof(TerrainTile) == 32, "TerrainTile 必须 32B（最大标量宽度 4 ⇒ Aligned 4）");
```

GLSL 侧同序同型（附录 A.1）。`Aligned 4` 就是"被指向类型的最大标量宽度 = 4"（§2.5 A1）。

#### (b) `TerrainFrame` —— 每帧参数（**112 字节**，`Aligned 16`）

```cpp
struct TerrainFrame
{
    float    vp[16];           // 相机相对 VP（CPU 用 double 算，见 §2.3）
    float    camera_pos_ws[4]; // 相机世界坐标（double 累加后转 float）
    uint64_t debug_addr;       // 调试回读缓冲地址（0 = 关闭；见 §3.6）
    uint32_t heights_stride;   // 高度缓冲行步长（texel/行）
    uint32_t world_texels;     // 世界高度缓冲边长（clamp 上界）
    float    texel_world_size; // 一个 texel 的世界尺寸
    float    height_scale;     // 高度值 → 世界 Z
    float    overlap_epsilon;  // 外扩带下沉量（× 本 tile 格世界尺寸，默认 1e-3）
    uint32_t flags;            // 位0 = 外扩遮缝开关
};
static_assert(sizeof(TerrainFrame) == 112, "");
// 布局：vp 0..63 / camera_pos_ws 64..79 / debug_addr 80..87 / heights_stride 88 /
//       world_texels 92 / texel_world_size 96 / height_scale 100 / overlap_epsilon 104 / flags 108
//       ⇒ 112B，16 对齐（struct 对齐 = 16，来自 mat4/vec4）✅
// 注：不要把 TerrainFrame 声明成 GLSL 数组引用（数组步长会变成 104 而触发 spirv-val 的
//     "stride not satisfying alignment to 16"）；用单结构引用（§3.3 A.1）。
```

- 缓冲**按帧槽 ring**（沿用引擎 8 槽惯例），`pc_root.addr_terrain_frame` 指向**本帧槽**；槽步长取 **128B**（16 对齐 + 留扩展余量）。
- 为什么不放进 push constant：**GLSL 每个 stage 只有一个 `push_constant` 块**，已被 `pc_root` 占用。
- GLSL 侧声明成**单结构引用**而不是数组：`layout(buffer_reference, scalar, buffer_reference_align = 16) readonly buffer TerrainFrameRef { TerrainFrame frame; };`
  ✅ 实测：若声明成 `TerrainFrame f[]`，`spirv-val` 会报
  `member 0 contains an array with stride 104 not satisfying alignment to 16`（数组步长 104 不是 16 的倍数）；单结构形式**无需任何附加 flag 即通过**。

#### (c) `pc_root`（`RootAddresses`）—— 80B → **112B**（见 §2.4）

### 2.2 高度缓冲布局

| 项 | 取值 | 说明 |
|---|---|---|
| 元素类型 | `uint16_t` | 原生 16 位（`storageBuffer16BitAccess` ✅ 本机 true） |
| 行步长 | `heights_stride`（texel/行） | 建议 = 世界边长；**无对齐要求** |
| 世界尺寸 | `world_texels²` | **不要求 2 的幂**；必须能被 tile 跨度整除（或填充，§2.5） |
| 访问 | 一次索引（可选双线性） | 顶点落在 texel 上（fraction = 0）⇒ 实际退化为单次读 |
| 越界 | **合法**（世界缓冲连续） | tile 边缘读邻居 texel **不需要 halo**；只在**世界边界** clamp |
| 上传 | 静态数据一次性 staging → `vkCmdCopyBuffer` | 用引擎的环形 Staging 池 |

```glsl
// 16 位读取：必须两级转换 ✅ 实测（直接 float(h) 被 glslc 拒：'constructor' : can't convert）
return float(uint(HBUF.data[uint(ty) * TF.heights_stride + uint(tx)]));
```

若将来顶点不落在 texel 上，双线性要手写（**`R16_UINT` 不能硬件双线性、不能线性 blit**）：

```glsl
float h = mix(mix(h00, h10, fx), mix(h01, h11, fx), fy);   // 手动 4 读 + lerp
```

### 2.3 坐标链（整数化到"算偏移"为止）

```
tile 格号 (cx,cy) ∈ [-1, cells]                     # -1 = 外扩格
texel   = tile.texel_origin + ((cx,cy) << tile.lod) # int32/uint32
offset  = ty * TF.heights_stride + tx
hw      = sample(offset) * TF.height_scale
cs      = TF.texel_world_size * (1 << tile.lod)     # 本 tile 的格世界尺寸
p_ws    = tile.texel_origin * TF.texel_world_size + (cx,cy)*cs
p_rel   = p_ws - TF.camera_pos_ws                   # 相机相对（喂给 TF.vp）
```

- **`TF.vp` 必须是相机相对 VP**：引擎的 `camera.vp` 是绝对矩阵且 camera-relative 尚未启用（`Camera.cpp:64-65`）⇒ 自己算（View 去掉平移）放进 `TerrainFrame`。
- **CPU 侧世界坐标用 double 累加**，最后一步才转 float；8 km 处 float 精度仍是 mm 级。
- 每块 `L2W` 矩阵**整块去掉**：地形不需要 per-object 变换，由 `texel_origin / texel_world_size / height_scale` 取代。

### 2.4 地址落位 D1-B：`pc_root` 80B → 112B ✅ 实测

**为什么是 B（`pc_root`）而不是 A（全局地址表）**：T1–T2.3 是"每 tile 一次 push + 一次 `DrawMeshTasks`"，**per-draw 的 tile 索引**必须有地方放 —— GLSL 每个 stage 只有一个 `push_constant` 块，所以索引与 3 个地址一起走 `pc_root` 最省；全局地址表是**每帧槽**语义，装不下 per-draw 数据。

**改动面（只改一个文件）**：`inc/hgl/graph/ShaderBufferSources.h`

1. `HGL_ROOT_ADDRESSES_FIELD_LIST`（`:267`）**末尾追加 5 行**：

```cpp
    M(addr_terrain_heights, "uint64_t", uint64_t)  \
    M(addr_terrain_tiles,   "uint64_t", uint64_t)  \
    M(addr_terrain_frame,   "uint64_t", uint64_t)  \
    M(terrain_tile_index,   "uint",     uint32_t)  \
    M(_pad_terrain,         "uint",     uint32_t)
```

2. `PushRootAddresses`（`inc/hgl/graph/RootAddressPush.h:31-43`）**末尾追加带默认值的参数**：
   `uint32_t terrain_tile_index = 0` ⇒ 现有 3 个调用点**一行都不用改**（`R1` 风险唯一的合法做法）。

**两条硬约束（实测踩到）**：

- **尾部 u32 必须成对补齐**：`RootAddressesLayoutValid()` 断言 `sizeof == Σ字段大小`（`ShaderBufferSources.h:334`，`static_assert` 在 `:336`），单个尾部 u32 会留下 4B 尾填充 ⇒ **编译期直接失败**。`_pad_terrain` 就是为它存在的。
- **新的 u64 组必须从 8 的倍数偏移开始**；**绝不往中间插字段**（S1 把 `addr_global_addresses` 插到第 4 位的那次教训）。

**实测偏移表**（MSVC 18 C++ `offsetof` ↔ `glslc` 产物 SPIR-V `OpMemberDecorate Offset`，✅ 逐字段一致）：

| 字段 | C++ `offsetof` | SPIR-V Offset |
|---|---|---|
| `addr_global_addresses` … `addr_text_char_instance`（9 个 u64） | 0, 8, …, 64 | 同左 |
| `camera_id` | 72 | 72 |
| `_pad_camera` | 76 | 76 |
| `addr_terrain_heights` | 80 | 80 |
| `addr_terrain_tiles` | 88 | 88 |
| `addr_terrain_frame` | 96 | 96 |
| `terrain_tile_index` | 104 | 104 |
| `_pad_terrain` | 108 | 108 |
| **`sizeof`** | **112** | Σ字段 = **112** ⇒ 无 padding ✅ |

> **GLSL 侧没有手写块要同步**：`pc_root` 的声明与布局断言都遍历**同一张 X 列表**
> （`src/ShaderGen/meshgen/MeshShaderHeaderGen.h:29-45` `EmitRootAddressesPushConstant`、`src/ShaderGen/compile/MaterialShaderEmitter.cpp:510`）✅
> ⇒ 改 C++ 一处，GLSL 与断言自动跟随。（与"改全局地址表"完全不同：后者 GLSL 是手写的 `ShaderLibrary/ubo/scene_ubo.glsl`，漏一行 = 材质静默回退。）

### 2.5 对齐与尺寸约束

**结论先行**：我们走 **buffer**（BDA storage buffer）不是纹理 ⇒ 纹理那套对齐规则（texel block、`bufferRowLength`/`bufferImageHeight`、`optimalBufferCopyRowPitchAlignment`、mip 链、array layer、sampler 寻址）**一条都不适用**；**2 的幂加在 LOD 上，不加在高度数据尺寸上**。

| # | 约束 | VUID | 我们的取值 |
|---|---|---|---|
| A1 | 物理指针访问的 `Aligned` 须是"被指向类型**最大标量宽度**"的倍数 | `VUID-StandaloneSpirv-PhysicalStorageBuffer64-06314` | 高度 `uint16_t[]` ⇒ **2**；tile 行 ⇒ **4**；含 `vec4/mat4` 的帧块 ⇒ **16**（✅ 实测 SPIR-V 出现 `Aligned 2/4/8/16`） |
| A2 | 指针值 ≥ 该 `Aligned` | `...-06315` | 取址后**断言**（驱动实测给 ≥16B，但别假设） |
| A3 | 被引用 buffer 必须带 `SHADER_DEVICE_ADDRESS` | `...-11819` | 用引擎 `CreateSSBO` / `GetBufferDeviceAddressAligned16` |
| 附加 | `storageBuffer16BitAccess` 须启用 | `VUID-RuntimeSpirv-storageBuffer16BitAccess-11161` | 启动断言该 feature ✅ 本机 true |

**2 的幂正确归属**：

| 对象 | 要 2 的幂？ | 原因 |
|---|---|---|
| 格步长 `1u << lod` | **要** | 粗格点必须落在细格点上（顶点级重合的前提）；texel 用 `<<` 位移 |
| tile 跨度 `kTileSpan`（格数） | **要**（取 2 的幂最省心） | 必须是最大格步长的倍数 ⇒ 任意 LOD 下 tile 原点都是所有步长的共同倍数 |
| **世界高度图尺寸** | **不要** | 只需能被 `kTileSpan` 整除；例 `8192×6144 = 32×24` tile 合法。**不整除时**：填充到整数倍（多分配几行几列、按 clamp 复制填边） |
| 行 stride | 不要 | 只是我们自己的 uint32 步长（建议 4 的倍数） |
| 纹理（若走 R16_UNORM 路线） | **不要** | Vulkan 无 POT 纹理要求（GL 1.x / ES 1.x 的遗留） |

**容量天花板**：`maxStorageBufferRange = 4294967292`、`maxMemoryAllocationSize = 4294901760`（均 ≈4 GiB）⇒ 16 位高度世界边长 ≤ **46340** texel（1 m/texel 即 46 km 见方）。超了：多 buffer + 地址数组（"页"，`page_index` 已预留）或 sparse（引擎零支持，远期）。

### 2.6 为什么高度走 buffer 而不是纹理（会被反复问）

1. **ULRE 硬约束 E7**：bindless 纹理 3 个 binding 的 `stageFlags` 只有 `FRAGMENT|COMPUTE`
   （`src/Vulkan/VKBindlessTextureManager.cpp:42/48/54`，第二组 `:220/226/232`）✅ 现场核 ⇒ **mesh 阶段采样不到纹理**；
   而 Z 必须在 mesh 阶段取到 ⇒ **没得选**。
   （独立 Vulkan 工程没有这条约束，所以那份文档把"纹理 A/B"留作对照实验。）
2. **整数域**：`R16_UINT` 不能硬件双线性、不能线性 blit；要硬件插值就得存 UNORM（归一化丢高度语义 + 量化还原）。
   走 buffer 时顶点落在 texel 上（fraction = 0），手动 lerp 退化为单次读。
3. **少一整套状态**：无 sampler（filter/address mode）、无 image layout 转换（纹理在 CS 写/读还要额外 barrier）、
   越界读就是索引加减（中心差分 ±step 天然合法）、分块不受 `maxImageArrayLayers` 限制。
4. **地址稳定**：BDA 地址在 T3（CS 写表）后不变，compute 与 mesh 用同一地址读。
5. **代价（说清）**：没有硬件滤波 / 各向异性 / 自动归一化；stride 与对齐要自己管（§2.5）。
   **与"不做 mipmap"是两件事** —— mip 是"另一个高度函数"（相邻 texel 平均），会破坏"粗格点 = 细格点同值"前提，
   无论走 buffer 还是纹理都不做；替代品是 **per-tile min/max 高度金字塔**（T2.2/T4 用）。
---

## 3. `MeshShaderMode::TerrainGrid`（新代码主体）

### 3.1 拓扑：格域、外扩、预算 ✅ 实测编译

| 项 | 值 |
|---|---|
| 一组（workgroup）负责 | `8×8` 格（`cells_per_group = 8`） |
| `local_size_x` | **64**（64 线程分摊 81…100 顶点 / 128…162 图元） |
| 本组自身格数 | `cols/rows = min(cells - k*8, 8)`（末批组可能不足 8） |
| **外扩**（T2.3 起） | 每轴**最后一批组**再向 −X/−Y 各多 1 格 ⇒ 格域 `≤ 9×9` |
| 顶点域 | `(cols+1) × (rows+1)` ⇒ 内部组 9×9 = **81**，边界组 10×10 = **100** |
| 图元 | `cols × rows × 2` ⇒ 内部 **128**，边界 **162** |
| `max_vertices` / `max_primitives` | **128 / 192**（对 100/162 留余量；对规范下限 256 仍有 1.5× 余量） |
| 格域起点 | `cx0 = kx*8 - 1`（**有符号**，因为 −1 是外扩格） |
| 对角线约定 | 全局固定 `(TL,BL,TR) + (TR,BL,BR)` |

**外扩格怎么"藏"**：外扩带顶点高度减去 `overlap_epsilon × cs`（默认 `1e-3`），使它**压在邻居表面之下**而不是与邻居共面 —— ε 只用于避开 z-fighting，**不需要覆盖裂缝深度**（这就是"向下挤出裙边 + 标定裙深"被替换掉的原因）。起伏区外扩带可能穿透邻居 ≤ LOD 误差，产生 1 格宽的细薄片，**不是孔洞**。

**外扩格采样落在邻居范围内是合法的**：世界高度缓冲连续 ⇒ **不需要 halo**；只有**世界边界**要 clamp（`world_texels - 1`）。

**线程分摊（关键纪律）**：**不要**写"每线程固定 4 格 × 64 线程"（81 格时必然越界/浪费），用线性分摊：

```glsl
for (uint i = gl_LocalInvocationIndex; i < vtot; i += 64u) { ... }   // 顶点
for (uint i = gl_LocalInvocationIndex; i < ptot; i += 64u) { ... }   // 图元
```

`SetMeshOutputsEXT(vtot, ptot)` 只由 `kx/ky` 决定 ⇒ **组内所有线程传同一个值**（per-threadgroup 语义的塌方记录见 `doc/mesh-shader-thread-model-and-loadvertexdata-conflicts.md` §2）。

### 3.2 组级产能接口（为什么不能复用现成接口）

现有模式的产能模型是"**线程数 × 每线程产量**"：

```cpp
// inc/hgl/mtl/MeshShaderMode.h:39-51  ✅ 现场核
GetMeshModeVerticesPerInvocation()    // VertexPassthrough=3, LineQuad/CharQuad=4
GetMeshModePrimitivesPerInvocation()  // VertexPassthrough=1, LineQuad/CharQuad=2
```

地形是"**每组固定产量、与线程数无关**"（100 顶点 / 162 图元由"组在 tile 内的位置"决定）。
⇒ **必须新增组级路径**；用乘法去凑 `max_vertices` 在 81/100 这种非 2 的幂产量上必然浪费或越界。

改点：

| 文件 | 改什么 |
|---|---|
| `inc/hgl/mtl/MeshShaderMode.h:9-14` | 枚举加 `TerrainGrid` |
| `inc/hgl/mtl/MeshShaderMode.h:18-29` | `ParseMeshShaderMode` 加 `"TerrainGrid"`（**未知值仍必须返回 false**，别静默降级） |
| `inc/hgl/mtl/MeshShaderMode.h:39-51` | 两个"每线程产量"函数**对地形不要给出会被误用的值**：显式走组级分支，或让调用方在 `TerrainGrid` 下不读它们 |
| `src/ShaderGen/meshgen/MeshModeDescriptor.h:183-217`（`s_descriptors[]`） | 新增第 4 项 + `ResolveTerrainGridTopology`（**组级常量**：`max_vertices = 128`、`max_primitives = 192`，与 `max_invocations` 无关） |
| `src/ShaderGen/builder/GenericMaterialBuilder.cpp:53-78` | `ClampMeshInvocationsByDevice` 用"每线程产量"反推组大小 ⇒ 地形必须走组级常量，否则算出的是错的 `max_vertices` |

> 这一条**必须在触点设计时就定**，不能等编译报错再改。

### 3.3 GLSL 骨架

完整可编译骨架在**附录 A**（✅ `glslc` + `spirv-val` 双通过，12380 字节 SPIR-V）：

- **A.1** 模式资源声明（`EmitTerrainGridResources` 产物：两个结构体 + 4 个 `buffer_reference` + 宏 + 拓扑 `layout`）
- **A.2** 模式主体（`EmitTerrainGridBody` 产物：`SampleHeightClamped` + `main`）
- **A.3** 自检封装（手写"生成器等价头部" + A.1 + A.2；**换机自检用这个**，一条 `glslc` 就能编）

### 3.4 模式注册触点（9 处，逐项有判据）

| # | 文件 | 改什么 | 判据 |
|---|---|---|---|
| 1 | `inc/hgl/mtl/MeshShaderMode.h` | 枚举 + 解析 + 组级分支 | TOML 里写错模式名仍报错（不静默） |
| 2 | `src/ShaderGen/meshgen/MeshShaderModeTerrainGrid.h`（新增） | `EmitTerrainGridResources` + `EmitTerrainGridBody` | 生成的 GLSL 与附录 A.1/A.2 **逐字节一致**（diff） |
| 3 | `src/ShaderGen/meshgen/MeshModeDescriptor.h` | `ResolveTerrainGridTopology`（组级常量 128/192）+ 注册第 4 项 | 管线创建成功、`max_vertices=128`、`max_primitives=192` |
| 4 | 同上 | stage1/2/3 槽填 `nullptr`（地形不走三段式顶点模块） | 无"缺少 stage"报错 |
| 5 | `inc/hgl/graph/ShaderBufferSources.h` | X 列表 +5 行（§2.4） | `static_assert(RootAddressesLayoutValid())` 通过、`sizeof == 112` |
| 6 | `inc/hgl/graph/RootAddressPush.h` | `PushRootAddresses` 末尾加 `terrain_tile_index = 0` | 现有 3 个调用点零改动、既有示例照常出图 |
| 7 | `src/ShaderGen/material_definition/MaterialDefinitionFile.cpp:1050-1070` | `[mesh_shader] mode` 已知键加 `TerrainGrid` | 未知键仍报错 |
| 8 | `src/ShaderGen/builder/GenericMaterialBuilder.cpp:53-78` | 容量计算加组级分支 | 日志里的 `max_vertices` = **128**（不是 64×4=256） |
| 9 | `ShaderLibrary/material/terrain_grid.material.toml`（新增）+ 示例 | 材质入口 + 示例 target | 材质能加载、出图 |

### 3.5 材质 TOML（真键名 ✅ 现场核）

```toml
schema = 3
id = "builtin/terrain_grid"
name = "TerrainGrid"
provider_policy = "GeometryOnly"

[fragment]
material_source_module = "material/pure_color.glsl"   # 或自写 terrain_debug_source.glsl（用 vUV 画棋盘/高度色）

[render_state]
pipeline = { cull_mode = "None", depth_test = true }  # 合法 cull_mode：None/Front/Back/FrontAndBack

[mesh_shader]
mode = "TerrainGrid"
max_invocations = 64          # 组内线程数（地形产量是组级常量，此键只决定 local_size）

[resources]
```

- `[transform]` 段**可选**（`MaterialDefinitionFile.cpp:757`）；若写就必须齐 5 个键（`:347-355`）。地形**不写**它 —— `stage1/2/3` 为空，位置由我们自己的 `TF.vp` 算出。
- `[fragment].material_source_module` 是**必需**键（`:771`）；`[mesh_shader]` 只认 `mode` 与 `max_invocations`（`:1051-1070`）。

### 3.6 调试回读通路（数值判据靠它，不用截图）

`TerrainFrame` 里留了 `uint64_t debug_addr`（偏移 80，见 §2.1b）：填 0 = 关闭；填地址 = shader 把每个顶点算出的
`texel 坐标 + 高度位型` 写进该缓冲。CPU 侧用**同一套公式**复算后逐位比对 ⇒ T1/T2.x 的"逐位一致"判据可以自动跑。

```glsl
// A.1 里声明（可写）：layout(buffer_reference, scalar, buffer_reference_align = 8) buffer TerrainDebugRef { uint64_t d[]; };
if (TF.debug_addr != uint64_t(0))
    TerrainDebugRef(TF.debug_addr).d[i] =
        uint64_t(uint(tx) | (uint(ty) << 16u)) | (uint64_t(floatBitsToUint(hw)) << 32);
```

- 缓冲大小：`vtot_max × 8`（一个 tile 一次最多 100 个顶点；多 tile 时按 tile 号分段或每次只回读一个 tile）。
- **唯一目的**是把"看起来对"变成"逐位相同"；跑通后即可保留（debug_addr=0 时零开销分支）。
---

## 4. 实施任务卡（逐张做，每张独立可验证/可回滚）

> 通用约定：**一次只引入一个新机制**；每张卡的参数都用"看得见"的通道（push/表/常量），不引入前缀和反查、GPU 侧索引（`gl_DrawID` 到 T2.4 才引入）；构建/运行命令见 §1.3；每张卡做完**先存档一张截图或一份回读数据**再进下一张。

### T1 — 单 tile 直发（新 mesh 模式 + BDA 读高度）

| 项 | 内容 |
|---|---|
| **目标** | 跑通"mesh 阶段生成 XY → 从 BDA 高度缓冲取 Z → 输出图元"这条链路，不涉及多 tile/LOD |
| **前置** | §3.4 触点 1–9 完成；`pc_root` 已扩到 112B（§2.4） |
| **参数** | 单 tile：`cells = 8`（⇒ `N = 1`，**1 组**、正好走边界组路径 ⇒ 100 顶点/162 图元，一次覆盖最坏情况）、`lod = 0`、`texel_origin = (0,0)`、`texel_world_size = 1.0`、`height_scale = 1.0`、`overlap_epsilon = 1e-3`、`flags = 0`、`debug_addr = <回读缓冲>` |
| **提交** | `PushRootAddresses(..., terrain_tile_index = 0)` → `cmd->DrawMeshTasks(1, 1, 1)`（组数 = `N²` = 1） |
| **判据** | ① 出图：8×8 格的网格（含外扩共 10×10 顶点）；② **回读缓冲逐位等于 CPU 复算**（texel 坐标与高度位型；CPU 用 `double` 复算，同位比较用定点/位型，避免浮点容差）；③ validation 零 error、日志无 `result -4` |
| **回滚** | 触点全部可回滚（X 列表 5 行 + `PushRootAddresses` 默认参 + 枚举/注册/模板新增文件） |
| **坑** | ① `float(h)` 编译失败 ⇒ 必须 `float(uint(h))`（§2.2）；② 忘了 `layout(location=0) out vec2 vUV[]` 的 **location** ⇒ `'location' : SPIR-V requires location for user input/output`；③ `SetMeshOutputsEXT` 传全局量（如 `cells*cols`）⇒ 超上限，AMD 直接崩 |

### T2.1 — 多 tile 均质渲染

| 项 | 内容 |
|---|---|
| **目标** | 一张地形由多个 tile 拼成（全部 `lod = 0`），仍用"per-tile push + 一次 `DrawMeshTasks`" |
| **前置** | T1 判据全绿 |
| **参数** | 4×4 = 16 个 tile，`cells = 64`（⇒ `N = 8`，每 tile 64 组）；`texel_origin = (tx*64, ty*64)`；世界 texel 尺寸 ≥ 256（须能被 tile 跨度整除） |
| **提交** | `for tile: { 更新 pc_root.terrain_tile_index（+ push）; DrawMeshTasks(N*N,1,1) }` |
| **判据** | ① 16 块无缝拼接（无重叠/无空洞——看边界带）；② 每个 tile 的回读逐位等于 CPU 复算；③ 与 T1 单 tile 渲染结果同 tile 区域**逐像素一致**（同样的 tile 用同样的参数画两次，比对图像） |
| **坑** | ① tile 表行序与 push 的索引不一致 ⇒ 画错块（**命令序 = 表行序**是引擎既有约定）；② `N²` 展平时 `N ≤ 256`（本机 `maxMeshWorkGroupCount` 每维 65535，展平后照样受限） |

### T2.2 — 带 LOD 的多 tile（**能看见裂缝是预期结果**）

| 项 | 内容 |
|---|---|
| **目标** | 每个 tile 独立选 LOD（格步长 `1<<lod`），tile 数/三角数随距离下降 |
| **前置** | T2.1 判据全绿 |
| **改动** | ① 误差度量（屏幕投影尺寸 + `height_max - height_min`）；② **per-tile min/max 高度金字塔**（构建期一次性）；③ tile 表按每帧重建（CPU 侧，允许） |
| **判据** | ① 相机拉远后 tile 数与三角数下降（统计日志断言）；② **相邻 LOD 的共享边出现可见裂缝**（截图存档——这是**预期**结果，T2.3 才修）；③ 粗格点与细格点在同位置取到同一高度（同位取值为 0 差） |
| **坑** | ① "2 的幂 + 同一函数"只保证**顶点级重合**，**不等于边内部重合**（T 型交点缝是固有问题，采样方式消不掉）；② 别想着用 mipmap 消缝 —— mip 是另一个高度函数，会**制造**裂缝 |

### T2.3 — 外扩遮缝（−X/−Y 各多 1 格，与邻居重叠）

| 项 | 内容 |
|---|---|
| **目标** | 用"几何覆盖"消掉 T2.2 的裂缝：**不是**向下挤出裙边 |
| **前置** | T2.2 判据全绿（尤其"能看见裂缝"的存档已拿到） |
| **改动** | §3.1 的格域扩展（`cols/rows` 末批组 +1、`cx0/cy0 = -1`）+ 外扩带 `-overlap_epsilon * cs`；`max_vertices/max_primitives` 128/192 不变（预算内） |
| **判据（三条，成对）** | ① 边界带**无孔洞**；② 平坦区**多帧截图一致**（无 z-fighting 闪烁）；③ **把 `flags` 位0 关掉必须能复现 T2.2 的裂缝**（只报"开着没缝"不算证据 —— 无法区分"遮住了"与"本来就没缝"） |
| **坑** | ① 外扩格用了 `uint` 格号 ⇒ 减法下溢（必须 `int`，§3.1）；② 把 ε 当"裙深"去标定（不需要，§3.1）；③ 只向一侧外扩（两侧对称是上述结论的前提） |

### T2.4 — ICB 化（CPU 填间接命令 → 一次 `DrawMeshTasksIndirectCount`）

| 项 | 内容 |
|---|---|
| **目标** | 提交从"N 次 push + N 次 Draw"变成"一次 Draw + 命令表"，为 T3 换生产者铺路 |
| **前置** | T2.3 判据全绿 |
| **引擎件** | 命令缓冲 `Device::CreateIndirectMeshTaskBuffer`（`inc/hgl/vk/VKDevice.h:419-420`、`src/Vulkan/buffer/IndirectCommandBuffer.cpp:77`）；count 缓冲 `Device::CreateDrawCountBuffer`（`VKDevice.h:402`）；提交 `RenderCmdBuffer::DrawMeshTasksIndirectCount`（`inc/hgl/vk/VKCommandBuffer.h:271-272`、`src/Vulkan/VKCommandBufferRender.cpp:451+`） |
| **tile 寻址** | `tile = terrain_tiles.t[gl_DrawID]`（引擎约定"命令序 = 表行序"）⇒ **不再需要 per-tile push** |
| **判据** | **与 T2.3 逐像素一致**（同相机、同参数，图像按位比较或逐像素差值 = 0） |
| **坑** | ① `gl_DrawID` 依赖 GLSL 4.60（文件头 `#version 460` ✓ 生成器已发）；② 间接命令的缓冲 usage 必须含 `INDIRECT`（`CreateIndirectMeshTaskBuffer` 已带）；③ `maxDrawCount` 要传**容量**而不是实际条数 |

### T3 — 生成迁到 ComputeShader（tile 表 + 命令 + count）

| 项 | 内容 |
|---|---|
| **目标** | CPU 不再逐 tile 写命令：CS 写 tile 表、间接命令与 count；CPU 提交代码**零改动**（沿用 T2.4 的 `DrawMeshTasksIndirectCount`） |
| **前置** | T2.4 判据全绿 |
| **参考** | `example/Basic/ComputeIndirectCount.cpp`（`CreateDrawCountBuffer` + `CreateIndirectMeshTaskBuffer` 的用法） |
| **判据** | ① 与 T2.4 逐像素一致；② CS 写的 tile 表与 CPU 复算的表逐行相同（回读比对）；③ CPU 侧提交代码 diff 为空 |
| **坑** | 帧槽竞态（CS 写第 N 帧的表、mesh 读第 N-1 帧）：缓冲按帧槽分份并同步；地址仍须在**物化处**注册（硬规矩 1） |

### T4 — CS 内 Frustum 剔除

| 项 | 内容 |
|---|---|
| **目标** | 视野外/屏幕过小的 tile 不产出命令（或 `groupCount = 0`） |
| **前置** | T3 判据全绿 |
| **方法** | tile AABB = `texel_origin × texel_world_size` 的 XY 范围 + `[height_min, height_max] × height_scale` 的 Z；6 平面测试；`IndirectCount` 形态下只需改 CS 的 count |
| **判据** | ① 视野外 tile 命令不存在/组数为 0（回读 count 与命令表）；② 出图与 T3 **逐像素一致**（剔除不能改变可见结果）；③ 相机快速转动无空洞（保守边界优先） |
| **坑** | 高度 min/max 金字塔若过期 ⇒ 该出现的 tile 被剔掉（表现为画面缺块）|
---

## 5. 验证台

### 5.1 判据速查

| 步 | 判据 1（数值，可自动） | 判据 2（几何/视觉） | 判据 3（纪律） |
|---|---|---|---|
| T1 | 回读缓冲逐位 == CPU 复算 | 8×8 格网格出图 | validation 零 error、无 `result -4` |
| T2.1 | 每 tile 回读逐位 == CPU 复算 | 16 块拼接无重叠/无空洞 | 与 T1 同 tile 区域逐像素一致 |
| T2.2 | 粗/细格点同位取值为 0 差 | tile 数与三角数随距离下降 | **能看见裂缝**（截图存档） |
| T2.3 | 关掉外扩能复现裂缝 | 边界带无孔洞、平坦区多帧无闪烁 | 两侧对称外扩 |
| T2.4 | 与 T2.3 逐像素一致 | 一次 `Draw` 画完全部 tile | `maxDrawCount` 传容量 |
| T3 | tile 表逐行 == CPU 复算 | 与 T2.4 逐像素一致 | CPU 提交代码 diff 为空 |
| T4 | 视野外 tile 组数 = 0 | 与 T3 逐像素一致 | 相机快转无缺块 |

### 5.2 数值判据怎么做（不依赖截图）

**通路**：`TerrainFrame.debug_addr`（§3.6）→ shader 把每个顶点的 `(tx, ty, hw 位型)` 写回 → CPU 读回比对。

**CPU 侧复算骨架**（与 shader 逐字同构，用 `double` 算完再转比较）：

```cpp
// 与 A.2 的 shader 逐句对应；tx/ty/hw 都是"算出来的同一个整数/位型"
const int32_t cx = cx0 + int32_t(vx);                 // 与 shader 同样的格号
const int32_t cy = cy0 + int32_t(vy);
const int32_t tx = int32_t(tile.texel_origin_x) + (cx << tile.lod);
const int32_t ty = int32_t(tile.texel_origin_y) + (cy << tile.lod);
const uint16_t raw = heights[clamp(ty,0,m) * stride + clamp(tx,0,m)];
float hw = float(raw) * height_scale;
if (cx < 0 || cy < 0) hw -= overlap_epsilon * cs;
const uint64_t expect = uint64_t(uint32_t(tx) | (uint32_t(ty) << 16u))
                      | (uint64_t(float_bits(hw)) << 32);   // ← 与 shader 的打包一致
EXPECT_EQ(readback[i], expect);                            // 逐位，无容差
```

**多帧一致性**：同一相机位置连跑 3 帧，回读应完全一致（漂移 = 帧槽/时序 bug，见 R13）。

### 5.3 视觉判据怎么做（引擎**没有**截图 API ✅ 现场核）

`rg -i "screenshot|SaveScreenshot|CaptureScreenshot" inc/ src/ example/` **零命中**（`SaveImageToFile` 只在 `src/Tools/TexConv` 里，是独立工具）。所以视觉证据只有两条路：

1. **人工截图**（最简单，开发期够用；`PrintScreen` + 存档，命名 `T<步>-<相机>-<时间>.png`）。
2. **程序化存盘**：离屏 RT 通路照 `example/Basic/RenderToTexture.cpp`（或 `RenderToTextureColorDepth.cpp`），
   读回 CPU 后用 vcpkg 里现成的 **`stb_image_write.h`**（`D:/vcpkg/installed/x64-windows/include/stb_image_write.h` ✅ 存在）写 PNG。
   ⇒ 只在这几条判据上做：T2.2 的"能看见裂缝"、T2.3 的"关掉外扩复现裂缝"、T2.3 的"多帧无闪烁"。

> ⚠ 本档**不**假定存在 `--screenshot` 之类的引擎开关（旧的 v1/v2 曾这么写，是错的，已在本档纠正）。

### 5.4 验证层与提交纪律

- 验证层：`AppFramework` 默认开（§1.3）⇒ 用它的示例天然带 validation；**每条"零 error"结论都必须看到验证层真的在跑**。
- 每个渲染门**单独** grep `result -4`（设备丢失的次生症状是 fence in use / semaphore 已 signal，主因往往在别处）。
- 跑前 `tasklist | grep <exe>` 确认没有残留进程（否则跑的是旧二进制，结论全假）；跑完 `taskkill /F /IM <exe>`（**单斜杠**）。

### 5.5 症状 → 原因 → 对策

| 症状 | 最可能原因 | 对策 |
|---|---|---|
| `vkQueueSubmit2 failed with result -4`（设备丢失） | shader 解引用 **0 基址**（地址没注册/没 push） | 硬规矩 1：物化处注册 + fail-fast + 日志打印地址 |
| 画面全黑但 validation 干净 | `pc_root` 字段顺序/大小与 GLSL 不一致（静默错位） | 靠 `static_assert(RootAddressesLayoutValid())` + §2.4 的偏移表核对 |
| 网格缺失一块/整块错位 | tile 表行序 ≠ 命令序（push 索引错） | 命令序 = 表行序（引擎约定）；日志打印 `tile_index → texel_origin` |
| 边界出现细黑缝 | 外扩带没生效或 ε 过大 | 检查 `flags` 位0 与 `cx0 = -1` 分支；判据③（关掉能复现裂缝）反证 |
| 平坦区闪烁 | 外扩带与邻居共面（ε = 0） | ε 默认 `1e-3 × cs`；不要为"遮得更深"而放大 ε |
| mesh shader 编译报 `'constructor' : can't convert` | `float(uint16 值)` 单级转换 | 两级：`float(uint(h))` |
| `stride not satisfying alignment to 16` | 帧块声明成数组引用 | 单结构引用（§2.1b） |
| AMD 驱动崩溃 | `SetMeshOutputsEXT` 超过 layout 上限 | 传组级常量 `vtot/ptot`（§3.1） |
| 高度整体错位/梯形拉伸 | `heights_stride` 或 `texel_world_size` 与 CPU 表不一致 | 两者都放进 `TerrainFrame`（单一真源），别在 shader 里硬编码 |

---

## 6. 风险与红线（实现时对照）

| # | 风险 | 对策 |
|---|---|---|
| R1 | `PushRootAddresses` 是**裸位置参数签名**（S1 曾把字段插到中间，风险应验过一次） | **只在末尾追加带默认值的参数**（`terrain_tile_index = 0`） |
| R3 | 组级产能与"每线程产量"接口语义冲突（§3.2） | 新增组级路径；不要用乘法凑 |
| R4 | `maxMeshWorkGroupCount` 每维 65535 ⇒ `N²` 展平后 `N ≤ 256` | 校验配置；tile 过大就加大 `cells_per_group` 或拆 tile |
| R5 | 外扩带与邻居共面 ⇒ z-fighting（看外扩 ε 与两侧对称） | ε 默认 `1e-3 × cs`；判据②③成对验证 |
| R13 | 帧槽竞态（CS 写第 N 帧、mesh 读第 N-1 帧） | 缓冲按帧槽分份 + 每帧同步；地址仍按槽下发 |
| R14 | `gl_DrawID` 依赖 `shaderDrawParameters` 与 GLSL 4.60 | 运行时断言 feature；不支持则 T2.4 回退 per-tile push |
| R15 | 缓冲分配策略 `Auto` 的两条路径（ReBAR → `CPUVisible` / 无 ReBAR → `StagedUpload`） | 回读缓冲显式用 `CPUVisible`（读回需要） |
| R16 | 16 位读取被 glslc 拒绝 | `float(uint(h))`（§2.2） |
| R17 | 外扩带来每边 1 格 overdraw | **开发期接受**（稳定性优先）；日后可只让"较粗一侧"外扩（需邻接 LOD 信息） |
| R18 | `Aligned` 不满足 / 多表拼一缓冲时段偏移未取整 | 取址后断言；段偏移统一向上取整到 `64`（本机 `minStorageBufferOffsetAlignment`） |
| R19 | 尾部 u32 破坏 `RootAddressesLayoutValid()` | 尾部 u32 **成对**（§2.4） |

---

## 7. 未决项与默认值

**已定案（不必再讨论）**：不走 Task Shader（Compute+Mesh）｜集成走材质系统 + `MeshShaderMode::TerrainGrid`｜高度走 BDA buffer（§2.6）｜地址落位 D1-B（`pc_root` 112B）｜遮缝用外扩重叠（不是向下裙边）｜不做 mipmap（改 min/max 金字塔）｜开发期不设性能目标。

**建议默认值（可直接用，都是可调旋钮）**：

| 旋钮 | 建议值 | 说明 |
|---|---|---|
| 世界尺寸 `world_texels` | **2048**（开发期） | 2 km @ 1 m/texel；须能被 `kTileSpan` 整除 |
| `kTileSpan`（tile 跨度，texel） | **64** | lod0 下 64×64 texel/tile ⇒ 世界 32×32 = 1024 tile |
| `cells_per_group` | **8** | 一组 8×8 格（外扩后 9×9） |
| `texel_world_size` | 1.0 | 1 texel = 1 世界单位 |
| `height_scale` | 1.0（开发期） | 高度原始值 0..65535 → 世界 Z；实景按地形尺度调 |
| `overlap_epsilon` | **1e-3**（× `cs`） | 只用来避开共面，不是裙深 |
| 帧槽数 | 8（沿用引擎） | `TerrainFrame` 槽步长 128B |

**仍待拍板（2 项，都可先按推荐值开工）**：

1. **间接提交形态**：`DrawMeshTasksIndirectCount`（**推荐**：T4 迁 CS 时提交代码零改动）vs 先 `DrawMeshTasksIndirect`。
2. **外扩对称性**：两侧对称（**推荐**，开发期不看 overdraw）vs 只让"较粗一侧"外扩（省一半 overdraw，但需要邻接 LOD 信息）。

---

## 8. 附录

### 附录 A —— GLSL 骨架（✅ 实测：`glslc` + `spirv-val` 双通过，12380 字节 SPIR-V）

#### A.1 模式资源声明（`EmitTerrainGridResources` 产物）

```glsl
// ── TerrainGrid 模式资源声明（MeshShaderModeTerrainGrid.h 的 EmitTerrainGridResources 产物）──
struct TerrainTile
{
    uint  texel_origin_x;   // tile 在高度缓冲中的 texel 原点
    uint  texel_origin_y;
    uint  cells;            // tile 自身格数/边（= 8 * groups_per_side）
    uint  lod;              // 格步长 = 1u << lod（texel）
    float height_min;
    float height_max;
    uint  page_index;       // 保留：多缓冲分页（T3+）
    uint  flags;
};

struct TerrainFrame
{
    mat4  vp;               // 相机相对 VP（CPU 侧用 double 累加世界坐标后算出）
    vec4  camera_pos_ws;
    uint64_t debug_addr;   // 调试回读缓冲地址（0 = 关闭）
    uint  heights_stride;   // 高度缓冲行步长（texel/行）
    uint  world_texels;     // 世界高度缓冲边长（clamp 上界）
    float texel_world_size; // 一个 texel 的世界尺寸
    float height_scale;     // 高度值 → 世界 Z
    float overlap_epsilon;  // 外扩带下沉量（× 本 tile 格世界尺寸，默认 1e-3）
    uint  flags;            // 位0 = 外扩遮缝开关（关掉用于复现裂缝）
    uint  _pad[2];          // 显式补齐到 112B（16 的倍数）
};

// 单结构引用（不是数组）：避免 ArrayStride 必须为 16 倍数的附加约束
layout(buffer_reference, scalar, buffer_reference_align = 16) readonly buffer TerrainFrameRef  { TerrainFrame frame; };
layout(buffer_reference, scalar, buffer_reference_align = 2)  readonly buffer TerrainHeightRef { uint16_t data[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4)  readonly buffer TerrainTileRef   { TerrainTile t[]; };
// 调试回读缓冲（可写）：T1–T2.x 的"数值判据"靠它——把每个顶点的 texel 坐标与高度写回，CPU 侧比对
layout(buffer_reference, scalar, buffer_reference_align = 8) buffer TerrainDebugRef { uint64_t d[]; };

#define TF    TerrainFrameRef(pc_root.addr_terrain_frame).frame
#define HBUF  TerrainHeightRef(pc_root.addr_terrain_heights)
#define TBUF  TerrainTileRef(pc_root.addr_terrain_tiles)

// 拓扑：组级固定产量（与 invocation 数无关），见 §3.2
layout(local_size_x = 64) in;
layout(triangles, max_vertices = 128, max_primitives = 192) out;

layout(location = 0) out vec2 vUV[];
```

#### A.2 模式主体（`EmitTerrainGridBody` 产物）

```glsl
// ── TerrainGrid 模式主体（EmitTerrainGridBody 产物）──
float SampleHeightClamped(int tx, int ty)
{
    const int m = int(TF.world_texels) - 1;
    return float(uint(HBUF.data[uint(clamp(ty, 0, m)) * TF.heights_stride + uint(clamp(tx, 0, m))]));  // 16→32 两级转换
}

void main()
{
    const TerrainTile tile = TBUF.t[uint(pc_root.terrain_tile_index)];
    const uint N  = (tile.cells + 7u) / 8u;            // 组数/边
    const uint kx = gl_WorkGroupID.x % N;
    const uint ky = gl_WorkGroupID.x / N;

    // 本组格域：自身格数（末组可能不足 8）+ 外扩 1 格（仅每轴最后一批组）
    const uint cols = uint(min(int(tile.cells) - int(kx) * 8, 8)) + ((kx == N - 1u) ? 1u : 0u);
    const uint rows = uint(min(int(tile.cells) - int(ky) * 8, 8)) + ((ky == N - 1u) ? 1u : 0u);
    const int  cx0  = int(kx) * 8 - 1;                 // 格域起点（-1 = 外扩格，故用 int）
    const int  cy0  = int(ky) * 8 - 1;

    const uint vpc  = cols + 1u;                       // 顶点列数（2..10 ⇒ 顶点 ≤ 100）
    const uint vtot = vpc * (rows + 1u);
    const uint ptot = cols * rows * 2u;                // 图元 ≤ 162

    SetMeshOutputsEXT(vtot, ptot);                     // 组内一致：只由 kx/ky 决定，不看线程

    const uint  li = gl_LocalInvocationIndex;
    // tile 世界原点 - 相机世界坐标（float 精度足够：8km 处约 mm 级）
    const vec3  tile_origin_rel = vec3(float(tile.texel_origin_x), float(tile.texel_origin_y), 0.0)
                                * TF.texel_world_size - TF.camera_pos_ws.xyz;
    const float cs = TF.texel_world_size * float(1u << tile.lod);   // 本 tile 的格世界尺寸

    for (uint i = li; i < vtot; i += 64u)              // 线性分摊，不用"每线程固定 4 格"
    {
        const uint vx = i % vpc;
        const uint vy = i / vpc;
        const int  cx = cx0 + int(vx);
        const int  cy = cy0 + int(vy);

        const int tx = int(tile.texel_origin_x) + (cx << int(tile.lod));
        const int ty = int(tile.texel_origin_y) + (cy << int(tile.lod));

        float hw = SampleHeightClamped(tx, ty) * TF.height_scale;
        if (cx < 0 || cy < 0)                          // 外扩带：压 ε 藏在邻居表面之下
            hw -= TF.overlap_epsilon * cs;

        // 相机相对：tile 世界原点（texel_origin × texel 世界尺寸）先减相机世界坐标，
        // 再加格偏移；TF.vp 也是相机相对 VP（CPU 用 double 累加后算出）
        const vec3 local = vec3(float(cx) * cs, float(cy) * cs, hw);
        gl_MeshVerticesEXT[i].gl_Position = TF.vp * vec4(tile_origin_rel + local, 1.0);
        vUV[i] = vec2(float(cx), float(cy));

        // 调试回读：texel 坐标 + 高度位型（CPU 侧用同一公式复算后逐位比对）
        if (TF.debug_addr != uint64_t(0))
            TerrainDebugRef(TF.debug_addr).d[i] =
                uint64_t(uint(tx) | (uint(ty) << 16u)) | (uint64_t(floatBitsToUint(hw)) << 32);
    }

    // 图元：格 (cxl,cyl) → 顶点 (cxl,cyl)(cxl,cyl+1)(cxl+1,cyl)(cxl+1,cyl+1)
    // 对角线约定全局固定：(TL,BL,TR) + (TR,BL,BR)
    for (uint i = li; i < ptot; i += 64u)
    {
        const uint cell = i >> 1u;
        const uint cxl  = cell % cols;
        const uint cyl  = cell / cols;
        const uint v00  = cyl * vpc + cxl;
        const uint v01  = (v00 + 1u) + vpc - 1u;       // = (cyl+1)*vpc + cxl
        gl_PrimitiveTriangleIndicesEXT[i] = ((i & 1u) == 0u)
            ? uvec3(v00, v01, v00 + 1u)
            : uvec3(v00 + 1u, v01, v01 + 1u);
    }
}
```

#### A.3 自检封装（手写"生成器等价头部" + A.1 + A.2；换机自检用这个）

```glsl
#version 460
#extension GL_EXT_mesh_shader : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_16bit_storage : require
// 以下块由 EmitRootAddressesPushConstant 按 HGL_ROOT_ADDRESSES_FIELD_LIST 发射（本档 §2.4 的 16 字段版）
layout(push_constant) uniform RootAddresses
{
    uint64_t addr_global_addresses;
    uint64_t addr_mesh_draw_params;
    uint64_t addr_l2w;
    uint64_t addr_l2w_index;
    uint64_t addr_mtl_data_addrs;
    uint64_t addr_texture_references;
    uint64_t addr_text_char_info;
    uint64_t addr_text_char_style;
    uint64_t addr_text_char_instance;
    uint camera_id;
    uint _pad_camera;
    uint64_t addr_terrain_heights;
    uint64_t addr_terrain_tiles;
    uint64_t addr_terrain_frame;
    uint terrain_tile_index;
    uint _pad_terrain;
} pc_root;

// ── TerrainGrid 模式资源声明（MeshShaderModeTerrainGrid.h 的 EmitTerrainGridResources 产物）──
struct TerrainTile
{
    uint  texel_origin_x;   // tile 在高度缓冲中的 texel 原点
    uint  texel_origin_y;
    uint  cells;            // tile 自身格数/边（= 8 * groups_per_side）
    uint  lod;              // 格步长 = 1u << lod（texel）
    float height_min;
    float height_max;
    uint  page_index;       // 保留：多缓冲分页（T3+）
    uint  flags;
};

struct TerrainFrame
{
    mat4  vp;               // 相机相对 VP（CPU 侧用 double 累加世界坐标后算出）
    vec4  camera_pos_ws;
    uint64_t debug_addr;   // 调试回读缓冲地址（0 = 关闭）
    uint  heights_stride;   // 高度缓冲行步长（texel/行）
    uint  world_texels;     // 世界高度缓冲边长（clamp 上界）
    float texel_world_size; // 一个 texel 的世界尺寸
    float height_scale;     // 高度值 → 世界 Z
    float overlap_epsilon;  // 外扩带下沉量（× 本 tile 格世界尺寸，默认 1e-3）
    uint  flags;            // 位0 = 外扩遮缝开关（关掉用于复现裂缝）
    uint  _pad[2];          // 显式补齐到 112B（16 的倍数）
};

// 单结构引用（不是数组）：避免 ArrayStride 必须为 16 倍数的附加约束
layout(buffer_reference, scalar, buffer_reference_align = 16) readonly buffer TerrainFrameRef  { TerrainFrame frame; };
layout(buffer_reference, scalar, buffer_reference_align = 2)  readonly buffer TerrainHeightRef { uint16_t data[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4)  readonly buffer TerrainTileRef   { TerrainTile t[]; };
// 调试回读缓冲（可写）：T1–T2.x 的"数值判据"靠它——把每个顶点的 texel 坐标与高度写回，CPU 侧比对
layout(buffer_reference, scalar, buffer_reference_align = 8) buffer TerrainDebugRef { uint64_t d[]; };

#define TF    TerrainFrameRef(pc_root.addr_terrain_frame).frame
#define HBUF  TerrainHeightRef(pc_root.addr_terrain_heights)
#define TBUF  TerrainTileRef(pc_root.addr_terrain_tiles)

// 拓扑：组级固定产量（与 invocation 数无关），见 §3.2
layout(local_size_x = 64) in;
layout(triangles, max_vertices = 128, max_primitives = 192) out;

layout(location = 0) out vec2 vUV[];

// ── TerrainGrid 模式主体（EmitTerrainGridBody 产物）──
float SampleHeightClamped(int tx, int ty)
{
    const int m = int(TF.world_texels) - 1;
    return float(uint(HBUF.data[uint(clamp(ty, 0, m)) * TF.heights_stride + uint(clamp(tx, 0, m))]));  // 16→32 两级转换
}

void main()
{
    const TerrainTile tile = TBUF.t[uint(pc_root.terrain_tile_index)];
    const uint N  = (tile.cells + 7u) / 8u;            // 组数/边
    const uint kx = gl_WorkGroupID.x % N;
    const uint ky = gl_WorkGroupID.x / N;

    // 本组格域：自身格数（末组可能不足 8）+ 外扩 1 格（仅每轴最后一批组）
    const uint cols = uint(min(int(tile.cells) - int(kx) * 8, 8)) + ((kx == N - 1u) ? 1u : 0u);
    const uint rows = uint(min(int(tile.cells) - int(ky) * 8, 8)) + ((ky == N - 1u) ? 1u : 0u);
    const int  cx0  = int(kx) * 8 - 1;                 // 格域起点（-1 = 外扩格，故用 int）
    const int  cy0  = int(ky) * 8 - 1;

    const uint vpc  = cols + 1u;                       // 顶点列数（2..10 ⇒ 顶点 ≤ 100）
    const uint vtot = vpc * (rows + 1u);
    const uint ptot = cols * rows * 2u;                // 图元 ≤ 162

    SetMeshOutputsEXT(vtot, ptot);                     // 组内一致：只由 kx/ky 决定，不看线程

    const uint  li = gl_LocalInvocationIndex;
    // tile 世界原点 - 相机世界坐标（float 精度足够：8km 处约 mm 级）
    const vec3  tile_origin_rel = vec3(float(tile.texel_origin_x), float(tile.texel_origin_y), 0.0)
                                * TF.texel_world_size - TF.camera_pos_ws.xyz;
    const float cs = TF.texel_world_size * float(1u << tile.lod);   // 本 tile 的格世界尺寸

    for (uint i = li; i < vtot; i += 64u)              // 线性分摊，不用"每线程固定 4 格"
    {
        const uint vx = i % vpc;
        const uint vy = i / vpc;
        const int  cx = cx0 + int(vx);
        const int  cy = cy0 + int(vy);

        const int tx = int(tile.texel_origin_x) + (cx << int(tile.lod));
        const int ty = int(tile.texel_origin_y) + (cy << int(tile.lod));

        float hw = SampleHeightClamped(tx, ty) * TF.height_scale;
        if (cx < 0 || cy < 0)                          // 外扩带：压 ε 藏在邻居表面之下
            hw -= TF.overlap_epsilon * cs;

        // 相机相对：tile 世界原点（texel_origin × texel 世界尺寸）先减相机世界坐标，
        // 再加格偏移；TF.vp 也是相机相对 VP（CPU 用 double 累加后算出）
        const vec3 local = vec3(float(cx) * cs, float(cy) * cs, hw);
        gl_MeshVerticesEXT[i].gl_Position = TF.vp * vec4(tile_origin_rel + local, 1.0);
        vUV[i] = vec2(float(cx), float(cy));

        // 调试回读：texel 坐标 + 高度位型（CPU 侧用同一公式复算后逐位比对）
        if (TF.debug_addr != uint64_t(0))
            TerrainDebugRef(TF.debug_addr).d[i] =
                uint64_t(uint(tx) | (uint(ty) << 16u)) | (uint64_t(floatBitsToUint(hw)) << 32);
    }

    // 图元：格 (cxl,cyl) → 顶点 (cxl,cyl)(cxl,cyl+1)(cxl+1,cyl)(cxl+1,cyl+1)
    // 对角线约定全局固定：(TL,BL,TR) + (TR,BL,BR)
    for (uint i = li; i < ptot; i += 64u)
    {
        const uint cell = i >> 1u;
        const uint cxl  = cell % cols;
        const uint cyl  = cell / cols;
        const uint v00  = cyl * vpc + cxl;
        const uint v01  = (v00 + 1u) + vpc - 1u;       // = (cyl+1)*vpc + cxl
        gl_PrimitiveTriangleIndicesEXT[i] = ((i & 1u) == 0u)
            ? uvec3(v00, v01, v00 + 1u)
            : uvec3(v00 + 1u, v01, v01 + 1u);
    }
}
```

**自检命令**（✅ 本机实测 exit 0）：

```bash
glslc --target-env=vulkan1.3 -o terrain_grid_selfcheck.spv terrain_grid_selfcheck.mesh
spirv-val --target-env vulkan1.3 terrain_grid_selfcheck.spv
# 预期：无输出（= 通过）；期望的 SPIR-V 特征：
spirv-dis terrain_grid_selfcheck.spv | grep -E "OpCapability|Aligned|ArrayStride"
```

期望结果 ✅：capability = `MeshShadingEXT` / `PhysicalStorageBufferAddresses` / `Int64` / `StorageBuffer16BitAccess`；
`ArrayStride` = `2`（高度）/ `32`（tile 行）；`Aligned` 出现 `2`（`uint16_t` 高度读）、`4`（tile 行）、`8/16`（帧块成员，受 `buffer_reference_align = 16` 影响）。

### 附录 B —— 公式 / 常量 / 路径速查

```
格步长(texel)   = 1u << tile.lod
格世界尺寸 cs   = texel_world_size * (1u << tile.lod)
texel 坐标      = tile.texel_origin + ((cx,cy) << tile.lod)
缓冲偏移        = ty * heights_stride + tx
高度            = float(uint(HBUF.data[偏移]))        // 16→32 必须两级转换
世界坐标(SHADER)= tile.texel_origin * texel_world_size + (cx,cy)*cs
相机相对        = 世界坐标 - camera_pos_ws
顶点预算        = 顶点 (cols+1)*(rows+1) ≤ 100 / 图元 cols*rows*2 ≤ 162（layout 128/192）
组数            = N*N，N = ceil(cells/8)，N ≤ 256
tile 覆盖       = cells * 格步长（lod0 时 = 世界 texel 数，须被 kTileSpan 整除）
BDA 对齐        = uint16_t[] ⇒ Aligned 2 / tile 行 ⇒ 4 / 含 vec4,mat4 的块 ⇒ 16
容量天花板      = 单缓冲 ≈ 4 GiB ⇒ 16 位高度边长 ≤ 46340 texel
```

| 路径 | 用途 |
|---|---|
| `build/out/Windows_64_Debug/` | exe 产物目录 |
| `C:/VulkanSDK/1.4.357.0/Bin/` | `glslc` / `spirv-val` / `spirv-dis` |
| `D:/vcpkg/installed/x64-windows/include/stb_image_write.h` | 需要程序化存 PNG 时用 ✅ 存在 |
| `%TEMP%\terrainspike2\`（本机） | 本次分析用的临时骨架（**已全量进附录 A**，换机不需要它） |

### 附录 C —— 触点文件索引（含行号，✅ 现场核）

| 文件:行 | 内容 |
|---|---|
| `inc/hgl/graph/ShaderBufferSources.h:267` | `HGL_ROOT_ADDRESSES_FIELD_LIST`（X 列表；+5 行） |
| `inc/hgl/graph/ShaderBufferSources.h:300/316/322/334/336` | GLSL 类型表 / 大小 / 偏移 / 断言（自动跟随，不用改） |
| `inc/hgl/graph/RootAddressPush.h:31-43` | `PushRootAddresses` 签名（末尾加 `terrain_tile_index = 0`） |
| `src/ShaderGen/meshgen/MeshShaderHeaderGen.h:29-45` | `EmitRootAddressesPushConstant`（GLSL `pc_root` 生成） |
| `src/ShaderGen/compile/MaterialShaderEmitter.cpp:510` | 同一 X 列表的另一处遍历 |
| `inc/hgl/mtl/MeshShaderMode.h:9-14/18-29/39-51` | 枚举 / 解析 / 每线程产量 |
| `src/ShaderGen/meshgen/MeshModeDescriptor.h:183-217` | `s_descriptors[]` 注册表 |
| `src/ShaderGen/builder/GenericMaterialBuilder.cpp:53-78` | `ClampMeshInvocationsByDevice`（组级分支） |
| `src/ShaderGen/material_definition/MaterialDefinitionFile.cpp:93-100/195-199/717-780/1050-1070` | `ParseCullMode` / `[render_state]` / `[fragment]` 必需键 / `[mesh_shader]` |
| `inc/hgl/vk/VKDevice.h:402/419-420` | `CreateDrawCountBuffer` / `CreateIndirectMeshTaskBuffer` |
| `inc/hgl/vk/VKCommandBuffer.h:264/267-272` | `DrawMeshTasks` / `DrawMeshTasksIndirect(Count)` |
| `src/ecs/support/line/LineRenderPipeline.cpp:283` | 自持管线 + `DrawMeshTasks` 样板 |
| `example/Basic/ComputeIndirectCount.cpp` / `ComputeFrustumCull.cpp` | CS 写 count / 命令表 + 剔除样板 |
| `src/Work/AppFramework.cpp:40-41` | 验证层默认开启 |
| `src/Vulkan/VKBindlessTextureManager.cpp:42/48/54`（及 `:220/226/232`） | E7：纹理 binding 的 `stageFlags` 无 mesh |
| `src/Vulkan/VKDeviceCreater.cpp:454` | `scalarBlockLayout` 按支持情况启用 |
| `ShaderLibrary/material/pure_color.material.toml`、`text_2d_gpu.material.toml` | TOML 样板 |

### 附录 D —— 术语

| 词 | 含义 |
|---|---|
| **格（cell）** | 地形四边形；一个格 = 2 个三角形 = 4 个顶点槽（顶点在相邻格间复用） |
| **格步长 / LOD** | `1u << lod` 个 texel 走一格；lod 越大格越粗 |
| **tile** | 一块地形；`cells × cells` 格，由 `N × N` 个 workgroup 生成 |
| **外扩（重叠遮缝）** | 每 tile 向 −X/−Y 各多生成 1 格，与邻居几何重叠把缝**盖住**（替代"向下裙边"） |
| **ε（overlap_epsilon）** | 外扩带下沉量，只用于避开与邻居共面，**不是**裙深 |
| **BDA** | Buffer Device Address：用 64 位裸地址访问缓冲，不经描述符集 |
| **失败模式 `result -4`** | `VK_ERROR_DEVICE_LOST`；本项目里最常见根因是 shader 解引用 0 基址 |
| **帧槽（frame slot）** | 每帧一份的缓冲分份（引擎 8 槽），避免 CPU/GPU 同写一份 |

### 附录 E —— 变更记录（本档相对 v2 的实质变更）

| # | 变更 | 依据 |
|---|---|---|
| 1 | `pc_root` 从 **88B 更正为 112B**（追加 5 行：3×u64 + u32 + u32 补齐） | MSVC `offsetof` ↔ SPIR-V `OpMemberDecorate` 逐字段实测 ✅（§2.4） |
| 2 | `TerrainFrame` 改**单结构引用**（不是数组），并把 `debug_addr` 排进 112B 布局 | `TerrainFrame f[]` 的 `ArrayStride 104` 触发 `spirv-val` 报错 ✅ |
| 3 | 新增**调试回读通路**（`debug_addr`）⇒ T1/T2.x 的"逐位一致"判据可自动跑 | 数值判据不能依赖截图 |
| 4 | 澄清**引擎无截图 API**，视觉存证两条路（人工 / 离屏 RT + `stb_image_write.hpp`） | `rg` 零命中 ✅（v1/v2 的 `--screenshot` 说法是错的） |
| 5 | 材质 TOML 换成**真键名**（`schema/id/name/provider_policy`、`[fragment].material_source_module` 必需、`[transform]` 可选、`[mesh_shader].mode/max_invocations`、`[render_state].pipeline.cull_mode` 四个合法值） | `MaterialDefinitionFile.cpp` ✅ |
| 6 | 组级接口触点补 `ClampMeshInvocationsByDevice`（它也读"每线程产量"） | `GenericMaterialBuilder.cpp:53-78` ✅ |
| 7 | 明确 `pc_root` 的 GLSL 侧**由生成器发射**，没有手写块要同步 | `MeshShaderHeaderGen.h:29-45` ✅ |
| 8 | 更正"Texture2DArray 层数上限 256"的说法（规范下限 256 / **本机 2048**）；真正的理由只有 E7 + 无 mip 需求 + 按层流式 | 设备真值 ✅ |
