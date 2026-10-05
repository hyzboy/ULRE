# ULRE 渲染流程：剔除 / 排序 / DrawCall 生成与提交（CPU 与 GPU 双路）——技术文档

> 版次：**v1（2026-10-05，HEAD = `0072145aa`，T3i 之后）**
> 关联文档：
> - 帧骨架、系统装配、RenderGraph、相位表 → `doc/ecs-layer-architecture-and-frame-flow.md`
>   （**其 §7.4/§7.5 的行号属旧修订、且写于 T3i 之前**；"多批切片 / 批标签 / GPU 接管"的现状以本档为准）
> - GPU 剔除线全过程（T3d→T3i、拍板记录、实测数字） → `doc/future/entity-rows-gpu-cull-plan.md` §8
> - 两级表 / 连号直通 / 4-ID 的历史设计 → `doc/render-item-descriptor-and-indirection-pipeline.md`
> - 描述符与地址契约（BDA 终态） → 帧流程档 §7.6
> 口径：本档只讲 **「实体 → `vkCmdDrawMeshTasks*`」**这一段（当前实现），不重复材质行物化、ShaderGen、
> 描述符/BDA 的既有内容。

---

## 0. 总览（一帧，标出 CPU / GPU 归属）

```
        ┌──────────────────────────── CPU（主帧命令缓冲） ─────────────────────────────┐
Tick    │ TickInput → TickTransform(L2W/AABB) → TickCamera(VP) → TickPostCamera       │
        └──────────────────────────────────────┬─────────────────────────────────────┘
                                               ▼
RenderCollect  ┌─ Collect：实体 → RenderItem（4-ID、world 矩阵、distanceToCamera）      CPU
               ├─ Cull(CPU)：视锥三层取精 → item->isVisible                            CPU
               └─ Sort(CPU)：① 项按 distanceToCamera 升序                             CPU
                                               ▼
RenderBatch    ┌─ TransformIndexing：L2W 组内下标（static/movable 分档）               CPU
               ├─ BuildMaterialBatches：按批键 {ShaderProgram*, Pipeline*} 归批        CPU
               ├─ FinalizeBatch：批内排序 → 索引/参数字表 → DrawBatch → ICB/四张表      CPU
               └─ TryApplyGPUCullOverride：逐批 guard；可接管批 → 标签/切片/清零计数    CPU（写表）
                                               ▼
RenderBufferCommit/Upload：本帧 CPU 写的表 → GPU（vkCmdCopyBuffer）                     CPU 录、GPU 搬
                                               ▼
RenderFrameSync ┌─ GPUCullSystem：一次 compute 派发（独立 cmd，异步提交）               GPU ★剔除+装配
               │    · 枚举实体行 → 视锥判定 → 全局幸存表 atomicAdd
               │    · 按批标签 → atomicAdd(批计数) → 写该批切片四流
               └─ CPUCullReferenceSystem：纯 CPU 参考 + Map 对拍（台架，按窗口采样）      CPU（校验）
                                               ▼
[渲染通道内] RenderDrawSubmit：逐批 → push constant → vkCmdSet*（EDS） → indirect 绘制  CPU 录、GPU 执行
```

两个关键位置（为什么在这儿）：

| 位置 | 原因 |
|---|---|
| **Cull / Sort 在 `RenderCollect`**（`PrimitiveCullSystem` / `PrimitiveSortSystem`，`src/ecs/support/primitive/*.cpp`） | 剔除结果 `isVisible` 是**建批的成员过滤条件** ⇒ 必须早于 `RenderBatch` |
| **guard 在 `RenderBatch` 末尾**（`TryApplyGPUCullOverride`，批建成之后） | 标签/切片/绘制归属三者必须**本帧同源**；批对象本身就是逐帧复用的批缓存 |
| **GPU 派发在 `RenderFrameSync`**（`GPUCullSystem::SetExecutionPhase`） | 在 `RenderBufferUpload` **之后**（标签/分区表已上卡）、渲染通道**之前**（绘制要读它写的四流） |

---

## 1. 目标与非目标

**目标**：把"剔除 → 排序 → DrawCall 生成 → 提交"的现状写全，含 **CPU 账**与 **GPU 接管（形态 B）**两条路、
两者的分流规则、以及可以照跑的判据。

**非目标**：① 材质行/纹理引用的物化过程；② ShaderGen 与 EDS 状态应用细节；③ DGC（`VK_EXT_device_generated_commands`）
的未来形态（本档只描述现状与预留点）；④ CPU 路径的性能优化（按拍板：现阶段不关心）。

---

## 2. 剔除

### 2.1 CPU 路（`PrimitiveBatchPipeline::PerformFrustumCulling`）

| 项 | 现状 |
|---|---|
| 枚举源 | `RenderFrameCache::renderItems`（**渲染项**，来自 Collect：一实体一项；④ 多实例项已整体删除（见 §6.3）⇒ **恒为 `PrimitiveRenderItem`**） |
| 视锥 | `frustum.SetMatrix(camera_info->vp)`（相机行来自世界级相机槽） |
| 判据（取精度） | ① 有世界 AABB ⇒ `Frustum::BoxIn(world_aabb)`；② 只有局部 AABB ⇒ 用 `GetCenter/GetExtents` 变换后测；③ 都没有 ⇒ 包围球（`TestFrustumWithWorldAABB` / `…LocalAABB` / `…BoundingSphere`） |
| 落点 | `RenderItem::isVisible`（bool）。**建批时按它过滤成员** ⇒ CPU 账里"批的 item 数 = 幸存者数"，命令条数 CPU 侧天生已知 |
| 代价 | 主循环 + 每项一次包围盒测试；无遮挡剔除（只有视锥） |

### 2.2 GPU 路（`ShaderLibrary/ecs/entity_gpu_cull.glsl` + `src/ecs/support/GPUCullSystem.cpp`）

| 项 | 现状 |
|---|---|
| 枚举源 | **实体行表**（世界私有 SSBO `EntityGPUTable`，`kCapacity = 4096`）：一线程一实体，`gl_GlobalInvocationID.x < pc.entity_count`；先过 `flags` 的 Active/Visible 位 |
| 事实源（零拷贝） | 实体 4-ID 行（`EntityRenderIDTable`）→ **池口径** `geometry_id` → `GeometryAABB` 池行（BDA 直读）。⚠ 不能用 `EntityGPU.row[2]`（那是组件池行号）当 geometry_id——这是 §8.2d 那个"两端同源的假绿"根因 |
| 矩阵/包围盒 | L2W 取 `l2w.mats[transform_id]`，AABB 池 + L2W 算世界中心与缩放，再做视锥判定 |
| 产出 ①（判决） | 全局幸存表：`atomicAdd(draw_count)` 得 `visible_index` ⇒ 供台架对拍（**不分批**，全实体口径） |
| 产出 ②（绘制装配） | 读 **批标签** `batch_tag[帧槽][entity_index]`：无效标签（`0xFFFFFFFF` / `≥ batch_count`）⇒ 不写行（该实体属未接管批）；有效 ⇒ `slot = atomicAdd(batch_counter[batch])`，把本实体写进**该批切片**的第 `slot` 行 |
| 切片四流 | 间接命令（Vulkan `VkDrawMeshTasksIndirectCommandEXT`，12B/行）、mesh 参数（`MeshDrawCommand`，8B/行）、`l2w_index`（4B/行）、材质行（`MaterialInstanceAddresses`，16B/行）——凸同 `row_base` 索引 |
| 溢出槽 | `slot >= capacity` ⇒ 跳过 + `atomicAdd(batch_counter[batch_count])`（**末位 = overflow**）；台架把 overflow ≠ 0 当硬红（3A 口径） |
| 派生/同步 | **一次派发覆盖全部实体**（workgroup 64）；T3g 起**异步**（独立 `ComputeCmdBuffer` 环形、无每帧 fence）；预算按 `HGL_FRAME_SLOT_TOTAL` 帧槽切份；派发末尾显式 `COMPUTE_SHADER → DRAW_INDIRECT` barrier，跨 cmd 的"cull 提交 → 主帧绘制提交"由**同队列 submission order** 保证 |

### 2.3 双路分流（门控与"跳不跳 CPU 剔除"）

- **opt-in**：`GPUCullSystem` 两个开关都不开 ⇒ 管线完全不触达渲染路径（引擎默认关）。
  - `SetEnabled(true)`：只派发 + 台架对拍（绘制仍走 CPU 账）；
  - `SetDrawOverrideEnabled(true)`（形态 B）：gather 输出**接管绘制**。
- **逐批 guard**（`TryApplyGPUCullOverride`，一条不满足只影响该批）：

| # | 前提 | 不满足的后果 |
|---|---|---|
| 1 | 批有 mesh stage | 该批不接管 |
| 2 | 非 `Lines`（X 组数公式不同源） | 该批不接管 |
| 3 | 接管批数 < `kMaxBatches`(256) | 超出的批不接管（告警一次） |
| 4 | 项有 owner 实体、实体 4-ID 行存在、`MeshDrawParams` 池行存在 | 该批不接管 |
| 5 | `MeshDrawParams.addr_meshlets == 0` | ② meshlet 几何：该批不接管 |

- **全量 / 部分接管**：只有**全量**接管才跳过 CPU 剔除（`IsGPUCullFullTakeoverActive()`）；部分接管时未接管批仍走 CPU 账。
  时序上"跳不跳"只能取**上一帧**的 guard 结果（guard 在建批之后评估）——稳态下批 = 全项、幸存数 ≤ 批容量 ⇒ 记账不被截。
- **实测**：全量 4/4（AlphaModes）；多批 2/2、1/1；**A3 台架**（`AM_PARTIAL=1`）自 ④ 删除（2026-10-05）后为 **5/5 全量接管**（`items=28`、`arena_rows=36`；异 program 自成一批 ⇒ 顺带覆盖「多批异 program」）；部分接管
  256/260（A4 台架 `CV_STRESS=260`：`arena_rows=1024`、`overflow=0`、`Σcount=256 < global=260`）。

---

## 3. 排序

**现状是一套"三层 + 一层缺"的结构：**

| 层 | 位置 | 规则 |
|---|---|---|
| ① 项（帧级） | `PrimitiveBatchPipeline::RunSorting → SortByDistance` | 全部渲染项按 `distanceToCamera` **升序**（远 → 近） |
| ② 批内 | `SortBatchItems` | 分**静态**（`!transform.IsMovable()`）与**可动**两组，各自按 `RenderItem::Compare()` 排序；静态在前；回写 `item->index`、记 `static_count`。**`gpu_driven_override` 批直接 return（不排序）** |
| ③ 批间 | `PrimitiveRenderSystem::OnRender` | 按 `MaterialBatch::key`（`ShaderProgramPipelineKey{shader_program, pipeline}`）排序后逐个提交 ⇒ 同 program 相邻、状态切换最少 |
| ④ 透明排序 | **不存在** | 没有按深度给混合材质排序的机制。`alpha_blend` 的真结果顺序 = ①②③ 的副产物；GPU 接管批的行序 = `atomicAdd` **到达序** ⇒ 与 CPU 账可不同。**像素逐字节判据只对不透明/镂空/抖动类硬，对混合排不硬**（"B1"，已知缺口） |

> 机制提醒：② 的"静态在前"是**为帧间稳定性**服务的（静态项不随相机重排 ⇒ 命令流稳定、属性更新量小）；
> 可动项仍按 `Compare()` 定序。别把这一层换成全局深度序——那会破坏 CPU 账与命令复用。

---

## 4. DrawCall 生成

### 4.1 批键（决定"哪些项能一起画"）

`MaterialBatch::key = ShaderProgramPipelineKey{ShaderProgram*, Pipeline*}`（只比两个指针）。
**EDS 之后 pipeline 只保留 shader 部分** ⇒ 改 blend/depth/cull/wireframe **不成批**；
要成新批必须是**不同的 shader program**：不同 `mtl_def_id` / 不同顶点节点（如 billboard）/ 不同覆盖模式（dither/alpha_test）等。
实测口径：AlphaModes 4 个 program ⇒ 4 批；同 def 但 alpha_cutoff 不同 ⇒ 每档一个 program（A4 台架用它造了 260 批）。

### 4.2 项定型（每个 item 的三个下标）

| 下标 | 谁写 | 含义 |
|---|---|---|
| `item->index` | `SortBatchItems` | **批内**序号（写回后即"行号"，供四流/属性表按批内序号定位） |
| `item->transform_index` | `AssignTransformIndices` | L2W 存储句柄：movable → `dynamic_base + group_index`，static → `group_index + 1`（**slot 0 恒单位矩阵**） |
| `item->distanceToCamera` | Collect（`RenderPrimitiveCollectSystem`） | 用于 ① 排序 |

### 4.3 DrawBatch 压缩（`BuildBatches`）

对批内项做区间折叠，产出 `DrawBatch{first_instance, instance_count, draw_key}`：

| 情形 | 处理 |
|---|---|
| 句柄**连号**（连续静态项 / 实例化项的多实例区间） | **连号直通**：一条 DrawBatch 覆盖区间（`is_direct`、`instance_count = N`），不展开成 N 条 |
| 句柄离散 | `CompactRenderItemHandles` 生成索引区间（索引表 + `instance_count`） |
| 混合（同几何同 range） | 同几何同绘制范围的相邻簇合并 |

DrawBatch 是**批内的绘制段**；批的所有段累积成**一条 multi-draw**（见 §5）。

### 4.4 四张表（"批 → 表"的核心）

| 表 | 行结构 | 行字节 | CPU 账（未接管批） | GPU 接管批 |
|---|---|---|---|---|
| 间接命令 | `VkDrawMeshTasksIndirectCommandEXT {X 组数, Y 实例数, Z=1}` | 12B | CPU 按幸存者写（`WriteMeshDrawCommands`） | **GPU 写**（冻结派发时 gather 阶段） |
| mesh 参数 | `MeshDrawCommand`（`gl_DrawID` 侧查表） | 8B | CPU 写 | GPU 写 |
| `l2w_index` | `transform_id` | 4B | CPU 写 | GPU 写 |
| 材质行 | `MaterialInstanceAddresses`（payload/texture + 阴影接收事实两位） | 16B | CPU 写 | GPU 写（**逐字节照抄实体行事实位**） |

**竞技场与对齐**：四张表共享同一片"行竞技场"，按帧槽切份。批的切片起点 `row_base` 必须
**`% 4 == 0`**（`row_cursor = (row_cursor + capacity + 3) & ~3`），因为同一行要同时满足三条引用的 16B 对齐
（命令 12B / 参数 8B / l2w 4B / 材质 16B 的公共倍数要求）；每批最多浪费 3 行。

**上限**：接管批数 ≤ `kMaxBatches = 256`；行竞技场容量 = `kMaxVisibleEntities = 4096`（与实体表容量同值）。
竞技场"累计项数超限"那条 guard 分支已按拍板删除（可达性证明见 `doc/future/entity-rows-gpu-cull-plan.md` §8.7 A4）。

---

## 5. 提交（一条批 = 一条 multi-draw）

```
                                 ┌── 未接管批：CPU 账
                                 │   条数 = 本批 DrawBatch 段数（CPU 已知）
   MaterialBatch ──► renderer ───┤   DrawMeshTasksIndirect(icb, first = 段起点, count)
     · key{pipeline,program}     │
     · draw_batches[]            │
     · gpu_row_base / capacity   └── 接管批：GPU 切片
     · icb_mesh_tasks  ─────────────► DrawMeshTasksIndirectCount(
     · icb_count_buffer                    icb,            first = row_base × 12B,
     · mesh_draw_params_buffer             countBuffer = batch_counter[batch],
     · l2w_index_buffer                    offset      = 该批计数偏移,
     · material_rows_buffer                maxDrawCount= capacity)
```

**每批一次的前置动作**（`PipelineMaterialRenderer::Render`）：

1. **push constant**（`RootAddresses` + 切片）：全局/世界地址、相机行、以及**四流在本批切片的地址**
   （`row_base × {8,4,16,…}` 的字节偏移，与 12B 命令流分开）；
2. **状态应用（EDS）**：`vkCmdSet*` 逐批设置 cull / depth test·write·compare / blend enable·equation / polygon mode /
   line width ⇒ **pipeline 只绑 shader**，渲染状态不参与批键、也不参与 pipeline 缓存；
3. **一条 multi-draw**（段累积后 flush）；批末重置段计数。

**判据一眼认法**（日志）：`[IndirectMeshDraw] mesh indirect flush engaged: first=… count=… (gpu_driven=… row_base=… capacity=…)`
——`gpu_driven=0` 走 CPU 账（`count` 是确切条数），`gpu_driven=1` 走 GPU 切片（`count` = `capacity` 上界，实际条数在 GPU 内存里）。

---

## 6. CPU / GPU 对照与判据

### 6.1 对照表

| 维度 | CPU 账（CPU Driven） | GPU 接管（形态 B，opt-in，默认关） |
|---|---|---|
| 剔除者 | `PrimitiveBatchPipeline::PerformFrustumCulling`（item 级） | `entity_gpu_cull.glsl`（实体行级，一次 compute） |
| 事实源 | 组件 `BoundingBox` / 世界 AABB | 实体 4-ID 行 → `GeometryAABB` 池（BDA 直读，零拷贝） |
| 粒度 | 批内项（已过滤） | 全实体（含未接管批！只是不给它们写行） |
| 排序 | ① 距离序 + ② 批内静态/可动 + ③ 批键 | 仅 ③；行序 = 原子到达序（①② 不做） |
| 命令来源 | CPU 写四流 | GPU 按切片写四流（指令流由 compute 写） |
| 条数 | CPU 已知 | 独立 countBuffer，CPU 不读回 |
| 记账 | —— | 全局幸存表 + 每批计数 + overflow 槽 |
| 校验 | —— | `CPUCullReferenceSystem` 纯参考 + 台架 Map 对拍；像素三方对拍 |
| 适用 | 现状全场景；保留作**调试参考**（未来 RELEASE 屏蔽由 C++ 宏完成） | 已覆盖：普通批 / 多批 / 混合材质 / 多覆盖模式；未覆盖：② meshlet |

### 6.2 台架与判据（这段流程怎么自证）

| 判据 | 看什么 | 合格线 |
|---|---|---|
| 门（离线） | `ShaderResourceSchemaRegressionGate` | **44 PASS / 0 FAIL** |
| 判决对拍 | `[CPUCullReferenceSystem] Parity: CPU=…, GPU=…, match=…, mismatch=0` | mismatch = 0 |
| 记账交叉 | `[T3i] batch slices: batches= rows= Σcount= global= overflow=` | `Σcount ≤ global`（切片只记被接管批）；`overflow == 0`（硬红） |
| 值链 | 同上行的 `值链=n/n bad=0`（实体行 + gather 行的阴影事实逐字节同源） | bad = 0 |
| 接管正向 | `[T3i] GPU cull draw override ENGAGED: batches=k/n items=… arena_rows=…` | k>0；全量时 k == n |
| 像素三方 | CPU 账 / 仅 dispatch / 接管 三次 dump（`*_DUMP=` 环境变量） | 逐字节全等（**混合排见 §3 ④**） |
| GPU 合法性 | VUID 计数 | = 0 |

> **负控是硬要求**：判据"绿"必须配上"能红"的证明。已实测：临时把竞技场容量改小 ⇒ `批次累计项数超共享竞技场` 精确顶格触发；
> 临时把批容量少一行 ⇒ `[T3i] 切片溢出！overflow=N` 真红。否则无法区分"判据绿"与"判据空转"。

---

## 7. 已知缺口与去留（与本文流程直接相关）

| 编号 | 内容 | 现状 |
|---|---|---|
| ② | **meshlet 几何接管** | guard 挡 `addr_meshlets != 0`；需给 GPU 一个可读的 meshlet 组数源（池行加字段或另表） |
| ④ | ~~实例化/多实例接管~~ **won't-do（2026-10-05）** | 查实**无真实使用者**（唯一活调用方是为「被拒」而存在的 A3 台架；上游没有「每实例不同」的生产者）⇒ 按零兼容**整体删除**：项类型 + 8 自由函数 + slot 实例三字段（行宽 48→40 B）+ guard 该条。详见 `entity-rows-gpu-cull-plan.md` §8.7 A2 |
| B1 | **混合排无深度排序** | §3 ④；GPU 接管批行序非确定 ⇒ 像素判据对混合排不保证 |
| —— | **DGC 不来支撑现状** | 开发机无 `VK_EXT_device_generated_commands`；形态 B 不依赖任何扩展。将来接 DGC 只换"执行层"，决策层/校验层零改动 |
| —— | **CPU 路径去留** | 保留（调试 + 未覆盖场景的唯一出路）；最终 RELEASE 由 C++ 宏屏蔽，取回校验常驻 |

---

## 8. 文件索引（按 `文件:符号`，避免行号腐烂）

| 环节 | 落点 |
|---|---|
| 相位定义 / 系统装配 | `inc/hgl/ecs/core/System.h:ExecutionPhase`、`src/ecs/core/DefaultSystems.cpp:InstallPrimitiveGroup` |
| 收集（渲染项、4-ID、距离、材质行物化） | `src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp` |
| 相位代理 | `src/ecs/support/primitive/PrimitiveCullSystem.cpp`、`PrimitiveSortSystem.cpp`、`PrimitiveBuildSystem.cpp`、`PrimitiveRenderSystem.cpp` |
| pipeline 门面 | `src/ecs/support/primitive/PrimitiveRenderPipeline.cpp`（`PrepareFrame/RunCull/RunSort/RunBuild`） |
| CPU 剔除 / 排序 / 建批 / guard | `src/ecs/support/PrimitiveBatchPipeline.cpp:PerformFrustumCulling / SortByDistance / SortBatchItems / BuildMaterialBatches / BuildBatches / WriteMeshDrawCommands / FinalizeBatches / TryApplyGPUCullOverride` |
| 批对象与键 | `inc/hgl/ecs/core/MaterialBatch.h`、`inc/hgl/ecs/core/ShaderProgramPipelineKey.h` |
| 提交与间接绘制 | `src/ecs/support/PipelineMaterialRenderer.cpp:Render / ProcIndirectRender` |
| GPU 剔除 / 切片 / 计数 / barrier | `src/ecs/support/GPUCullSystem.cpp`（`Dispatch`、`ApplyGPUCullOverrideHook`）、`ShaderLibrary/ecs/entity_gpu_cull.glsl` |
| 台架与对拍 | `src/ecs/support/CPUCullReferenceSystem.cpp` |
| 状态应用（EDS 逐批） | `src/Vulkan/VKCommandBufferRender.cpp:RenderCmdBuffer::ApplyPipelineState` |

---

## 9. 验证矩阵（照跑即可）

| 场景 | 命令要点 | 期望 |
|---|---|---|
| 离线正确性 | `ShaderResourceSchemaRegressionGate.exe` | 44 PASS / 0 FAIL |
| CPU 账基线 | 示例直接跑（不开 cull） | `[ERROR]`=0、VUID=0 |
| 仅派发（判决） | `<示例>_CULL=1` | Parity mismatch=0；绘制不变 |
| 形态 B 接管 | `<示例>_FORM_B=1` | `ENGAGED batches=k/n`；Pixel 三方全等 |
| 多批 | `PBR_MULTIBATCH=1` / `CoverageVariants`（2 批）/ `AlphaModes`（4 批） | `batches=2/2`、`4/4` |
| 部分接管 | `CV_STRESS=260 CV_DIST=60`（CoverageVariants，批数超限）/ meshlet 批（②） | `batches=256/260`、`Σcount < global` |
| 批数上限 | `CV_STRESS=260 CV_DIST=60`（CoverageVariants） | 告警 1 次、`batches=256/260`、`arena_rows=1024` |
| 像素对拍 | `*_DUMP=<tag>` 三次（CPU / dispatch / 接管） | 逐字节全等（混合排除外） |

---

## 附录 A：术语

| 词 | 含义 |
|---|---|
| 批（MaterialBatch） | 一个 `{ShaderProgram*, Pipeline*}` 键下的全部待绘项；键相同才可能一起画 |
| 切片（slice） | 接管批在共享行竞技场里的连续行区间，`{row_base, capacity}` |
| 四流 | 间接命令 / mesh 参数 / `l2w_index` / 材质行，同一 `row_base` 索引 |
| gather | GPU 剔除阶段把幸存实体"装配"进本批切片的动作 |
| 形态 B | 无扩展依赖的 GPU 驱动路径（compute + mesh shader + indirect count） |
| 台架 | `CPUCullReferenceSystem` + `GPUCullSystem` 的 Map 采样对拍（生产路径零同步） |

## 变更记录

| 版本 | 变更 |
|---|---|
| v1（2026-10-05） | 首版：整理 T3i 之后的剔除 / 排序 / DrawCall 生成与提交现状，含 CPU 与 GPU 双路对照、A3/A4 实测口径、文件索引与验证矩阵；并标注帧流程档 §7.4/§7.5 为旧修订 |