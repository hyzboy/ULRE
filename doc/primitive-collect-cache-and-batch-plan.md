# 主帧收集快路径与批计划重放（P0–P5）

> 适用代码：`src/ecs/systems/render/collect/RenderPrimitiveCollectSystem.cpp`、
> `src/ecs/support/pipeline/PrimitiveBatchPipeline.cpp`、
> `inc/hgl/ecs/support/storage/*`（FrameProfiler / PrimitiveTopologyLedger / MainFrameTopologySnapshot）
> 压测入口：`example/Benchmark/01_ChessboardStress.cpp`（729 棋盘 × 4 部件 + 350 骰子）
> 校验入口：`example/Benchmark/ChessboardStressValidate.cmake`

## 1. 起因：为什么"只变 TRS"也要每帧全量收集

`RenderFrameCache::BeginFrame()` 每帧每个 pass 都会清空 `items` / `materialBatches` /
`geometryBatches`。这是刻意的设计：批对象里装的是 `PrimitiveRenderItem`，而 item 里存的是
**指向共享池行、当前帧有效的指针/下标**，跨帧持有它们会把"池行被回收复用"变成悬垂引用。
于是即使一个场景完全静止，`RenderPrimitiveCollectSystem` 每帧仍要走完整收集循环。

在 `01_ChessboardStress` 的定档规模（1079 可见图元 / 3266 可见部件）下，这一段的成本
**全部来自读映射过的 GPU 缓冲**：

| 读的是什么 | 位置 | 单次成本 |
|---|---|---|
| `MeshDrawParams` 行的字段 | 全局 SSBO 池（host-visible、非缓存） | ≈2.3 µs / 字段 |
| `ModelTemplateRow` / `ModelPartRow` 的映射 | 同上 | ≈2.3 µs / 字段 |
| `EntityRenderIDTable::Row()` | 同上 | 同上 |

**这是全篇最重要的结论**：缓存**行指针**没有用（SKIP 实验里缓存指针只把 15.7ms 变成
15.4ms），必须缓存**字段值**。P3 的差分实验给出决定性证据：只把 6 个 draw 字段的值缓存
下来，收集循环从 15.7ms 掉到 1.74ms。

## 2. 四层机制

| 层 | 提交 | 作用 | 是否改变行为 |
|---|---|---|---|
| P0 FrameProfiler | `dd2df2fba` | 在真实调用边界打点，子槽记账 | 否（纯观测） |
| P1 拓扑失效台帐 | `54a1010be` | 覆盖全部"会改变拓扑的输入"的单调代数 | 否（纯观测） |
| P2 主帧拓扑快照 | `877b734b7` | 双槽原子提交的 item/批快照 + 对拍 | 否（只构建+对拍） |
| P3 主帧收集快路径 | `21ac99419` | 快照有效时跳过整个收集主循环 | **是**（受开关控制） |
| P4 批计划重放 + 模板事实缓存 | `9d1089298` | 批键表/项归属复用、模板映射记忆化 | **是**（无开关，始终生效） |
| P5 四路校验 | `471ccd528` | 快/慢路径像素对拍 + 稳态判据入 CTest | 否（测试） |

### P1：`PrimitiveTopologyLedger`

两个维度、各自单调递增：

- `TopologyChange`：`EntityStructure` / `GeometryBinding` / `ModelTemplate` /
  `MaterialBinding` / `PassAndTarget`
- `DynamicFactChange`：`Visibility` / `PartMask` / `CustomData` / `PrevTransform` / `MaterialContent`

写入方必须是**所有**会改变对应输入的代码路径；漏一个写者，快路径就会带着陈旧快照跑。
这就是为什么压测台有"拓扑/内容对拍"这一栏——它靠独立重算发现漏网写者。

### P2：`MainFrameTopologySnapshot`

双槽（`pending` / `live`）+ 原子提交：先在 pending 槽里构建完整快照，成功后一次性切换
`live_slot`；构建中断则丢弃 pending，live 保持上一版。**绝不允许半成品上线**。

### P3：收集快路径

`TryCachedMainFrameCollect()` 的四道前提：

1. P2 快照就绪，且 `成员数 == 上一帧收集数`
2. 总拓扑代数自快照构建后未变
3. GPU 剔除已接管（`GpuCullNotEngaged` 时不能用别人的剔除结果）
4. `last_main_collect_steady` 为真（材质链已就绪）

通过后按快照重建 item 与事实（`PublishPrimitiveFacts()`），并**逐项校验**成员仍然存在
（实体在、几何组件在、变换在、材质共享行未被回收）；任一校验失败即**回退**到慢路径。
"回退"计数非 0 就意味着台帐漏了写者。

`PublishPrimitiveFacts()` 用 8 参 `draw_seed`（而不是从 `MeshDrawParams` 行里读）——
这正是第 1 节结论的直接应用。

### P4-A：模板部件事实缓存

`TryApplyGPUCullOverride()` 的桶聚类循环里，每个模板项都要展开一次
`ModelTemplateRow` → `ModelPartRow` 映射，只为拿到槽数与槽几何。改按 `template_id`
记忆化 `TemplatePartFacts`（`part_offset` / `part_count` / `part_bucket_count` /
`slot_count[]` / `slot_geometry[]`），失效键 = `(拓扑代数, 池内容代数)`。

顺带修掉一个真 bug：原实现用**共享**的 `part_slot_count` / `part_slot_geometry` 草稿，
同一批里出现第二个模板时会覆盖第一个模板的槽计数 ⇒ 多模板批容量算错。现在每个模板
持有独立事实。

### P4-B：材质批计划重放

`BuildMaterialBatches()` 每帧的真正开销是**解析管线**（`PrimitiveRenderItem` 的
`GetShaderProgram()` / `GetPipeline()` / `GetPipelineConfig()` 各一次
`unordered_map` 查找，每项 3 次）。批对象本身必须逐帧重建，但**键表**和
**项→批归属**可以复用。

`BatchPlanSlot`（**每个 render pass 一份**，上限 8 槽）记录：批键表、项→批下标、
拓扑代数、材质事实代数、项序列指纹（entity index / generation / isVisible 按序 FNV1a）、
可见项数。四条命中条件全部满足即按计划重建批，不解析任何管线；否则走原慢路径，
失败时**整体不提交**计划（拒绝半成品）。

> 槽位是按 `RenderPass*` 分的，不是单槽。首版用单槽时 Main / Shadow 两个 pass 每帧
> 互相覆盖，表现为 hits 持续增长但批其实每帧都在重建。

## 3. 开关与默认值

| 环境变量 | 默认 | 含义 |
|---|---|---|
| `ULRE_MAINFRAME_TOPO_CACHE` | **关闭** | 构建 P2 快照（纯观测，不动执行路径） |
| `ULRE_MAINFRAME_COLLECT_CACHE` | **关闭** | 允许快照接管收集（真正改执行路径，要求上一项也开） |

取值：`1` / `t` / `T` / `y` / `Y` 开头为开，其余（含 `0`）为关。

**为什么默认关闭**：P3/P4 改的是执行路径，而它们依赖"台帐覆盖全部拓扑写者"这个
不变量。该不变量目前只在压测台的对拍与像素摘要上验证过（一个世界、一套渲染工作流）。
默认关闭意味着**出问题只需 unset 环境变量即可回到原路径**；等更多场景（地形、粒子、
多 pass、编辑器视口）跑过 P5 的四路对拍，再考虑翻默认。

P4 没有开关：它不改变任何可观测语义（批的对象、顺序、内容完全一致），只是复用键表，
因此不提供关闭路径（否则等于给自己留一条永远不会被测的代码分支）。

压测台自身的等价开关：`CSTRESS_TOPO=1` ≡ `ULRE_MAINFRAME_TOPO_CACHE=1`，
`CSTRESS_COLLECT_CACHE=1` ≡ `ULRE_MAINFRAME_COLLECT_CACHE=1`。
**`CSTRESS_COLLECT_CACHE=1` 单独用无效，必须同时 `CSTRESS_TOPO=1`。**

## 4. 读报表

`CSTRESS_PROFILE=1 CSTRESS_TOPO=1 CSTRESS_COLLECT_CACHE=1`，采样帧打印：

- `CollectCacheReplay` 子槽 —— 快路径重建 item 的耗时（慢路径这里是 0）
- `P3 主帧收集快路径` 报表 —— `接管=N (≈1.00/帧)` / `回退=0` / `稳态=yes`
- `P4 材质批计划重放` 报表 —— `重放=N (≈1.00/帧)` / `计划未命中≤1`（暖机帧建计划）

排查顺序：**回退非 0 → 拓扑/内容对拍不一致 → 接管率 < 1 → 计划未命中持续增长**。

⚠ `P1 失效台帐` 那一栏的"增量/帧"是从进程启动累计的，包含了**场景装配期**的
几千次拓扑变更（本例 ≈57/帧 × 170 帧）。判断稳态要看同一栏里的
`拓扑末次变更帧`（`#<init>` / `#1` 表示装配后没再变过），不要看"增量/帧"。

## 5. 实测

Release，1280×720，`CSTRESS_FRAMES=180 CSTRESS_SAMPLE=170 CSTRESS_PROFILE=1`，
同一台机器同一次会话前后脚跑：

| 口径 | 收集 cache 关 | 收集 cache 开 |
|---|---|---|
| 帧时间 | 44.72 ms（22.4 fps） | 17.80 ms（56.2 fps） |
| `CollectPhase` | 38.72 ms | 2.44 ms |
| ├ `CollectPreScan` | 9.56 ms | 0.08 ms |
| ├ `CollectMainLoop` | 29.15 ms | 0.57 ms |
| └ `CollectCacheReplay` | 0 | 1.78 ms |
| `BatchPhase` | 0.58 ms | 0.57 ms |
| ├ `BatchBuildMaterial` | 0.039 ms | 0.033 ms |
| ├ `BatchGPUCullGuard` | 0.533 ms | 0.490 ms |
| └ `BatchFinalize` | 0.541 ms | 0.537 ms |
| 可见图元 | 1073 | 1073 |
| 溢出槽 | 0 | 0 |

开 cache 的那次运行有 7.7 ms"未归因"（`<- 仍有未打点段落`）：此时 CPU 侧已不再是
瓶颈，差额是呈现/等待时间落在了打点区间之外，不是新增开销。

分列看 P4 的贡献（缓存开、改前 → 改后）：

| 子槽 | P4 前 | P4 后 |
|---|---|---|
| `BatchBuildMaterial` | 2.95 ms | 0.02 ms |
| `BatchGPUCullGuard` | 2.51 ms | 0.05 ms |
| `BatchFinalize` | 2.55 ms | 0.09 ms |

## 6. 正确性保证

`ChessboardStressValidate.cmake` 跑四遍：

1. 摘要自检 + cache **关** → 基线 `fnv1a64`
2. 摘要自检 + cache **开** → 摘要必须与①**逐字节一致**
3. 常规运行（cache 关）→ `溢出槽=0` + `done frames=N`
4. 稳态运行（cache 开）→ `回退=0`、`稳态=yes`、`接管≥1`、`计划未命中≤2`

当前基线：`fnv1a64=0xC53E2E0E20AB269E`（`ULRE_NO_INSTANCING=1` 时为
`0x966D6FDBBE188383`）。Release 不钉死该值（浮点/优化差异），只要求两条路径互等。

## 7. 已知残余风险

- `MaterialRuntimePipelineTable` 内部是 `unordered_map<EntityID,Entry>`，**没有版本计数器**。
  P4-B 的失效判据假设"改这个表必然推动 `MaterialBinding` 拓扑代数或
  `DynamicFactChange::MaterialContent` 代数"。若有代码路径绕过台帐直接改表，
  批计划会陈旧。→ 需要新增此类写者时，必须同时推台帐。
- 快照是**单世界一份**（世界隔离天然成立），但 render pass 计划槽上限 8；超过会整体清空
  退化为慢路径（不会错，只会慢）。
