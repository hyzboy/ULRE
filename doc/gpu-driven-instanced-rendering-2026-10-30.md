# GPUDriven 实例化渲染（Instanced Multi-Draw）——实施方案与落地记录

> 状态：**已实现**（2026-10-30）。本文是设计与落地结果的单源说明，实现细节以代码注释为准。

## 1. 问题

GPUDriven 路径改造前，`entity_gpu_cull.glsl` 为**每个幸存实体**写一条
`VkDrawMeshTasksIndirectCommandEXT{ groupCountX, 1, 1 }`：

- `groupCountY`（实例轴）恒为 **1** ⇒ 一次 `DrawMeshTasksIndirect` 的 N 条命令
  实际是 **N 次单实例绘制**，硬件实例化通路完全没被用到；
- `PBRSpheres`（10×10 = 100 个球，10 种几何，1 个材质批）实测 = **100 条命令**；
- CPU 侧其实**已经知道**哪些实体同素材（同一 `geometry_id` / 同一管线），只是
  布局行（`BatchInfoRow`）的最小单位是"整个 MaterialBatch"，没有素材维度的细分。

## 2. 方案：把布局行的最小单位从「批」下沉到「素材桶」

### 2.1 CPU：批内按 `geometry_id` 聚类成素材桶

`PrimitiveBatchPipeline::TryApplyGPUCullOverride` 在原有逐项前提校验的同时，
把同一 `MaterialBatch` 内的项按 `geometry_id` 线性聚成**素材桶**（`GeometryBucket`）：

- 桶数 = 本批出现过的几何种类（典型个位数），线性查表、无额外分配；
- 桶集合与桶序只由「批键序 + 批项序」决定 ⇒ **帧间稳定**（标签/布局/命令行都稳定）；
- 一个桶占**一行**布局（`BatchInfoRow`），`row_base` 语义变为**实例行起点**，
  `capacity` = 桶容量（= 该几何体的项数）；
- 实例行按桶连续分配，桶间推进仍走 `(cursor + capacity + 3) & ~3` 的 4 行对齐。

### 2.2 两套编号空间（关键）

实例化后"一条命令 N 个实例"，命令与实例不可能再共用同一个行号。因此切片
（`MaterialBatch::GpuSlice`）拆成两套互相独立的编号空间，分别落在**不同的 GPU 缓冲**上：

| 空间 | 缓冲 | 下标 | 用途 |
| --- | --- | --- | --- |
| **命令空间** | `IndirectCommands` (ICB) + `CmdsOut` | `BatchBlockBase(pass) + bucket` | 每桶一条命令；`gl_DrawID` 是桶在**本批**内的相对序号 |
| **实例空间** | `L2WOut` + `MtlOut` | arena **绝对**行号 | 每实例一行；`gl_InstanceIndex` 直接就是行号 |

绘制侧据此下发：

- `first = cmd_base × 12`（ICB 命令基址）、`maxDrawCount = cmd_count`（= 桶数）；
- `PushRootAddresses` 的 `mesh_draw_params_offset = cmd_base × 8`，
  `l2w_index_offset = mtl_data_addrs_offset = 0`（实例表走绝对行号）。

`gl_InstanceIndex = draw_params.first_instance + gl_WorkGroupID.y`（既有宏，
`mesh/stage_common_head.glsl`）保持不变：`first_instance` 现在 = 桶的实例行起点。

### 2.3 GPU：实例数原子聚合，组数由着色器现算

`entity_gpu_cull.glsl` 对幸存实体：

```
slot      = atomicAdd(counters[block_base + bucket], 1)
inst_row  = binfo.row_base + slot                 // 实例行
cmd_index = block_base + bucket                   // 命令行
icb[cmd_index].groupCountX = 组数（现算，同值幂等写）
icb[cmd_index].groupCountZ = 1
atomicAdd(icb[cmd_index].groupCountY, 1)          // 实例数：只能原子累加
cmds[cmd_index] = { geometry_id, binfo.row_base }  // first_instance = 桶实例起点
l2w[inst_row] / mtl[inst_row] = 逐实例数据
```

- `groupCountX` 仍由**着色器**用「元素数 / 组大小」现算（保持"组数不在 CPU 侧另立一套账"的既有契约）；
- `groupCountY` 必须原子累加 ⇒ 每帧由 cull 命令缓冲内的 `vkCmdFillBuffer` **先清零**
  （ICB 因此新增 `TRANSFER_DST` usage + TRANSFER→COMPUTE 屏障）；
- 零幸存桶的 `groupCountX` 保持 0 ⇒ 命令被实现忽略，无额外开销。

### 2.4 容量常量

`GPUCullSystem::kMaxBatches` 的语义随之下沉为**素材桶数**（不再是批数），
由 256 提到 **1024**（覆盖"整场景几何种类数"）。相应缓冲：
`BatchInfo` = slots × 2 × 1025 × 16 B ≈ 192 KB，`BatchCounter` ≈ 48 KB（帧槽分份）。

## 3. 效果

| 场景 | 改造前命令数 | 改造后命令数 |
| --- | --- | --- |
| `PBRSpheres`（1 批 / 10 几何 / 100 项） | 100 | **10**（每条最多 10 实例） |
| `BasicLitMeshes`（12 个互不相同几何） | 12 | 12（**自然退化**：每桶 1 项） |
| `AlphaModes`（4 批 / 28 项） | 28 | 4 |

`PBRSpheres` 实测日志：
`[T5] GPU cull instanced draw ENGAGED: batches=1/1 buckets=10 items=100 arena_rows=120`。

## 4. 兼容与验证

- **自然退化**：批内几何各不相同时 `buckets == items`，行为与逐项绘制等价；
- **A/B 诊断开关**：`ULRE_NO_INSTANCING=1` ⇒ 每项独立成桶（capacity=1），
  等价于改造前的"每项一条单实例命令"，用于逐像素对拍与后续回归定位；
- **像素等价证据**（Debug，冻结相机 + 定帧摘要，`FrameDigest`）：

  | 台架 | 改造前 fnv1a64 | 改造后（实例化开） |
  | --- | --- | --- |
  | `PBRSpheres` | `0x6A0B0B1B110D68D7` | **一致** |
  | `AlphaModes` | `0x19AA540ECA9F6CB6` | **一致** |
  | `CoverageVariants` | `0xC6DB3E16151A95D0` | **一致** |
  | `PlaneGrid3D` | `0xE0D4EC9DA58B8693` | **一致** |
  | `AtmosphereSkyMinimal` | `0x7B1289E90CBC377B` | **一致** |
  | `CascadeShadowMap`（阴影 pass） | `0x1508D2AB901E445F` | **一致** |
  | `RenderToTextureColorDepth`（离屏单帧世界） | `0xE8C0E8815C232AFF` | **一致** |
  | `GPUDrivenCull` | `0xCA7417EBC7B42E78` | **一致** |
  | `DualWorldShadows`（自检 PASS） | PASS | **PASS** |

- `IBLEnvironment` 的帧摘要**跨构建不稳定**（同一未改动分支上，
  `ULRE_NO_INSTANCING` 对照构建的摘要也≠基线）：该例的 `DeferredResource`
  异步装配跨越前若干帧，"第 8 帧回读"因此对时序敏感，**不构成实例化回归判据**。
  其余 8 个台架（含 7 个在不同构建间像素逐字节一致）已覆盖多批、阴影 pass、离屏单帧世界。

## 5. 后续可做（未纳入本次）

1. **桶内实例排序**：目前实例在桶内按原子顺序落位（渲染序无关紧要，但不利于后续
   GPU 侧 LOD / 深度预排序）；
2. **按 `geometry_id` 全局聚类**（跨 MaterialBatch）：`cmds` 行的 `geometry_id` 已与
   材质解耦，若把材质 ID 也搬进实例行，可进一步把同几何不同材质的项并进同一命令
   （需要材质/纹理行表按实例解析，属更大改动）；
3. **`kMaxBatches` 与竞技场行数的联动**：`kArenaRowsPerPass`（4096）现在是"每 pass
   可见项数上限"，实例化不再放大它；若要做更大规模仍需分块 dispatch。

## 6. 多材质整模型回归：GPUModelMultiMaterial

`example\Instancing\GPUModelMultiMaterial.cpp` 使用三个共享实例，每个实例为一个真正的
三部件、两 Shading Group 模板，不挂代表性 `GeometryData`。另有普通图元用于验证
自动归一化后的几何、材质变更。Unlit 部件不投影，StandardOpaque 替换模板负责阴影验收。

正式多材质入口 `BuildMultiMaterialModelTemplate` 先检查当前资源域、必需几何与材质运行期行，
全部 Ready 后才分配并发布；Pending 不分配、不替换旧模板，Failed 回滚已取得的行。
低层占位模板接口保持兼容。纹理仍遵守已有策略：流送中等待，加载失败走既有 fallback，
显式 `ClearAsyncRequest()` 后可以重试；就绪更新使用当前有效纹理配置，不改共享部件的初始 ID。

整实例覆盖通过 `GPUModelRefComponent::SetOverrideMaterialRow` / `ClearOverrideMaterialRow`
获取及释放引用，并触发失效。动态二级路由按 `(pass, template_id, override row)` 区分，
每槽为 `uvec4(桶基址, 有效材质 ID, 有效纹理 ID, 有效位)`，CPU 选批与 GPU 展开使用同一来源。
模板销毁拒绝活跃实例引用；解除引用后延迟到所属世界的 GPU 提交完成再回收部件和材质引用。
主帧与阴影分别缓存每组程序及管线，不能互相覆盖。

从仓库根目录运行（路径按实际构建目录替换）：

```powershell
cmake --build build --config Debug --target GPUModelMultiMaterial
cmake -DEXAMPLE=E:\ULRE\build\out\Windows_64_Debug\GPUModelMultiMaterial.exe -DRUNTIME_DIR=E:\ULRE -P example\Instancing\GPUModelMultiMaterialValidate.cmake
```

驱动执行三遍：缓存关、缓存开、缓存开且实例反向创建；逐阶段比较 51 份像素摘要，同时检查
GPU 部件材质/纹理/桶路由、provider 热切换、阴影深度开关及四轮退休复用。
负例还包括必需资源 Pending、活引用销毁拒绝、部件池耗尽回滚，以及两张纹理的确定性失败后重试。
预期错误逐条限定次数并过滤，其余渲染错误仍使验证失败。每遍最多 240 帧、120 秒；
日志及失败回读写入运行驱动时的当前目录，不应加入版本控制。

`GMM_SELFCHECK=1` 开启自检，`GMM_CASE=all` 为完整序列，`core` 仅验证 Unlit 核心流程及生命周期。
`GMM_FRAMES` 设置帧预算，`GMM_REVERSE_CREATE=1` 反向创建共享实例，`GMM_DUMP` 指定失败回读路径。
普通模式不注入失败，默认同样有有限帧预算。

CTest 使用独立开关 `-DULRE_ENABLE_GPU_MODEL_MULTI_MATERIAL_TEST=ON`，测试名为
`GPUModelMultiMaterialIntegration`，要求真实 Vulkan 设备和可创建窗口的会话，不将无设备跳过算作通过。

模板组保留部件的 `PrimitiveType`；不同拓扑不能合并为同一组，单图元模板去重键同样包含拓扑。
材质程序的变体键及构建请求必须使用组内拓扑，不能固定为 `Triangles`，否则线段包围盒会按三角形解释。
`RBB_SELFCHECK=1` 为 RenderBoundBox 的限帧检查：核对实际包围盒程序为 `Lines`，并回读三帧。
`example\Debug\RenderBoundBoxValidate.cmake` 通过 `EXAMPLE` / `RUNTIME_DIR` 参数运行缓存关/开对拍；
可用 `-DULRE_ENABLE_RENDER_BOUND_BOX_TEST=ON` 注册 `RenderBoundBoxIntegration`。
