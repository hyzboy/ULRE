# RenderStagePipeline / ECS 渲染系统重构计划（2026-02-21）

## 实施进度（滚动）

- 2026-02-21 / Phase 1（进行中）
- ✅ `RenderStages` 已移除 ECS 业务调用，收敛为 pass 编排。
- ✅ `BuildEcsPipeline` 已精简为纯图形 stage 链路。
- ✅ `RenderStageContext` 已补充边界注释（`ecs_context` 仅兼容）。
- ✅ `ECSContext::Render` 已补充入口语义保护日志（非侵入式）。
- ✅ `RenderFrameSystem` 已标记为 legacy 候选并从默认注册路径排除。
- 2026-02-21 / Phase 2（进行中）
- ✅ `RenderPrimitiveBatchSystem` 已拆出阶段化步骤接口（RunCulling/Sorting/TransformIndexing/Batching）。
- ✅ 新增 RenderPrimitive* 拆分系统（默认未启用，受开关控制）。
- ✅ `RenderPrimitiveBatchSystem` 已由 PrimitiveBatchPipeline + 拆分系统替代。
- 2026-02-21 / Phase 3（进行中）
- ✅ 已新增 TextCollectSystem / TextBuildSystem / TextResourceSyncSystem。
- ✅ TextRenderPipeline 已落地并接入 ECSContext，Text 拆分系统改为直接调用 pipeline。
- ✅ TextRenderSystem 已从默认注册与构建中移除，并删除旧实现文件。
- 2026-02-21 / Phase 4（进行中）
- ✅ RenderSystemCore 已移出 camera/environment begin-frame 业务同步，改由 ECSContext 承载（行为保持不变）。
- ✅ RenderSystemCore 已移出 BeginFrame 阶段编排细节（BeginFrame/BufferCommit/BufferUpload/PostBegin），统一委托 ECSContext::PrepareRenderPassSetup。
- ✅ SwapchainNextImage / Submit 编排已迁移到 ECSContext::Render(float)（保留 system-first + fallback 行为），RenderSystemCore 仅保留 begin/end 壳层。
- ✅ RenderTarget viewport/resize 同步检查已迁移到 ECSContext::SyncRenderTargetViewport，RenderSystemCore 不再直接处理该业务。
- ✅ RenderPreBeginFrame 调度已迁移到 ECSContext::Render(float) 入口，RenderSystemCore 不再直接调用该 phase。
- ✅ begin-frame 业务同步 helper 已系统化：新增 RenderFrameBusinessSyncSystem（RenderPostBeginFrame），并移除 RenderBeginFrameBusinessSync 特判路径。
- ✅ PrepareRenderPassSetup 已收敛为纯 phase 编排接口（移除 cmdBuffer 参数），进一步去除非 phase 特判痕迹。
- ✅ 已为关键同/跨 phase 链路补充显式依赖：QuadResourcePrepare -> RenderTarget、QuadMaterialBinding -> QuadResourcePrepare、RenderFrameBusinessSync -> RenderBufferUpload/Environment。
- ✅ 已补第二批跨阶段依赖：RenderPrimitiveSubmit/TextRenderSubmit -> RenderBufferUpload，SwapchainSubmit -> PrimitiveSubmit/TextSubmit/LineRender，进一步降低对注册顺序的隐式耦合。
- ✅ RenderSystemCore 已进一步壳层化：BeginFrame 仅建立 frame command，BeginRenderPass 独立；current_render_cmd 与 pre-pass phase 编排改由 ECSContext::Render(float) 管理。
- ✅ 增加 RenderSystemCore 帧状态保护（render_pass_begun）：BeginRenderPass 失败时可安全 EndFrame 收尾，避免 frame 状态卡死。
- 2026-02-21 / Phase 6（完成）
- ✅ 已产出最终交付文档：`doc/refactor/render_pipeline_ecs_final_delivery_2026-02-21.md`（包含最终边界定义、迁移对照表、回归结论）。

## 1. 背景与问题定义

当前渲染链路存在三类核心问题：

1. **双入口与职责重叠**
   - `RenderSystemCore` 与 `BuildEcsPipeline(RenderStagePipeline&)` 都在做 BeginFrame 前后流程编排。
   - `RenderStagePipeline` 已具备完整 stage 架子，但主路径仍由 `ECSContext::Render(float)` + `RenderSystemCore` 驱动。

2. **系统粒度不均衡**
   - `RenderPrimitiveBatchSystem` 与 `TextRenderSystem` 职责过大（CPU 收集、排序/批次、GPU 资源写入、生命周期管理混合）。

3. **语义与实现漂移**
   - `RenderFrameSystem` 只有头文件设计，无对应落地实现与注册。
   - 文档中“RenderStage 可承载完整流程”的描述与运行时事实不一致，导致团队协作认知偏差。

---

## 2. 重构目标（必须满足）

### 2.1 架构目标

- **单一帧驱动入口**：只允许一个生产路径负责 frame begin/end 与 submit。
- **清晰边界**：
  - `RenderStagePipeline`：只负责“RenderTarget/RenderPass 编排（图形编排层）”。
  - ECS Systems：负责“世界数据准备、渲染数据构建、draw 提交（业务渲染层）”。
- **Phase 语义稳定**：系统行为必须与 `ExecutionPhase` 名称一致。

### 2.2 工程目标

- 可分阶段迁移，不一次性重写。
- 每阶段可独立回滚。
- 关键行为（swapchain acquire/present、文本渲染、线框叠加）无功能回退。

### 2.3 性能与可观测性目标

- 不引入额外 CPU 热路径退化（目标：重构后帧 CPU 开销变化控制在 ±5%）。
- 每个阶段增加可观测日志（phase 次序、关键计数、失败点）。

---

## 3. 非目标（本轮不做）

- 不改 Vulkan 后端底层接口语义。
- 不引入全新渲染特性（deferred/clustered 等）。
- 不进行全仓库命名大清洗（仅处理本计划涉及模块）。

---

## 4. 目标职责边界（最终态）

## 4.1 RenderStagePipeline（图形编排层）

仅允许包含以下能力：

- RenderTarget 状态检查与切换（尺寸、viewport 同步）。
- `BeginRender` / `BeginRenderPass` / `EndRenderPass` / `EndRender`。
- 可插拔 pass 组合（例如场景捕获、离屏 pass）。

禁止在 Pipeline Stage 中直接做：

- ECS component 遍历与业务逻辑处理。
- Camera/Environment 等 ECS 系统数据同步。
- Swapchain acquire/present 的系统语义决策。

## 4.2 ECS 渲染系统（业务渲染层）

保留并强化：

- `RenderCollect`：组件收集、可见性决策。
- `RenderBatch`：排序/批次构建。
- `RenderDrawSubmit`：draw command 提交。
- `RenderPreBeginFrame/BufferCommit/BufferUpload/RenderSubmit`：资源更新与帧级提交。

## 4.3 Frame Driver（帧驱动壳层）

建议保留 `RenderSystemCore` 作为壳层，并收敛其职责：

- 做：帧生命周期推进（Begin/End/错误处理/状态保护）。
- 不做：业务同步细节（例如 camera/env 同步、collect/batch 业务逻辑）。

---

## 5. 当前系统到目标层的映射

| 当前模块 | 当前主要职责 | 目标归属 | 处理策略 |
| --- | --- | --- | --- |
| RenderSystemCore | Begin/End + 多项业务同步 | Frame Driver | 保留，逐步瘦身 |
| RenderStagePipeline + RenderStages | Stage 编排 + ECS调用混合 | Pipeline | 移除 ECS 业务调用，仅保留图形编排 |
| SwapchainNextImageSystem | Acquire | ECS (`RenderSwapchainNextImage`) | 保留 |
| SwapchainSubmitSystem | Present/Submit | ECS (`RenderSubmit`) | 保留 |
| RenderBufferCommitSystem | Commit Queue | ECS (`RenderBufferCommit`) | 保留 |
| RenderBufferUploadSystem | Upload + Barrier | ECS (`RenderBufferUpload`) | 保留并加强依赖说明 |
| RenderPrimitiveCollectSystem | 图元收集 | ECS (`RenderCollect`) | 保留 |
| RenderPrimitiveBatchSystem | 剔除/排序/批次/GPU编码 | ECS（拆分后多系统） | 拆分 |
| RenderPrimitiveSubmitSystem | 批次绘制提交 | ECS (`RenderDrawSubmit`) | 保留 |
| TextRenderSystem | 文本收集+资源+布局+几何 | ECS（拆分后多系统） | 拆分 |
| TextRenderSubmitSystem | 文本绘制提交 | ECS (`RenderDrawSubmit`) | 保留 |
| LineRenderSystem | 线框提交 | ECS (`RenderPostProcess`) | 保留 |
| QuadResourcePrepareSystem | 共享资源准备 | ECS (`RenderPreBeginFrame`) | 保留 |
| QuadMaterialBindingSystem | 每实体材质绑定 | ECS (`RenderPreBeginFrame`) | 保留 |
| RenderFrameSystem | 仅头文件设计 | 待决策 | 与 RenderSystemCore 二选一 |

---

## 6. 重构策略（两阶段原则）

采用 **“先清边界，再拆大系统”**：

1. 第一阶段：不改功能，只消除重复入口与职责越界。
2. 第二阶段：对超大系统拆分，按 phase 组织流水线。

这样可以把风险拆到可控子问题，避免一次性大爆炸。

---

## 7. 分阶段实施计划（详细）

## Phase 0：基线冻结与观测

**目标**：建立重构前可对比基线。

**任务**：

1. 增加帧级日志开关：记录每帧各 phase 顺序、系统耗时、关键计数。
2. 固定测试场景：
   - 普通 mesh + 文本 + line overlay。
   - 带 resize 的 swapchain 场景。
3. 导出 baseline 数据：
   - 平均帧时、P95 帧时。
   - collect 数量、batch 数量、draw call 数。

**验收标准**：

- 能稳定输出 200+ 帧统计且无崩溃。
- 同一输入场景多次运行结果波动在可接受范围（建议 <10%）。

---

## Phase 1：入口收敛与边界硬化（无行为改动）

**目标**：明确“谁驱动帧，谁做业务”。

**任务**：

1. 明确生产入口为 `ECSContext::Render(float)` → `RenderSystemCore`。
2. 在 `RenderStages` 内标记并禁用 ECS 业务路径（如 `ctx.ecs_context->RenderBufferCommit/Upload/...`）。
3. 文档同步：修正“Pipeline 负责完整帧”的表述为“Pipeline 负责 pass 编排”。
4. `RenderFrameSystem` 做架构决策：
   - 方案 A：删除/废弃。
   - 方案 B：作为 `RenderSystemCore` 的替代实现进行迁移（本计划推荐 A，先减复杂度）。

**验收标准**：

- 主流程唯一且可追踪。
- RenderStages 不再触发 ECS 业务同步。
- 功能输出与基线一致（截图/计数一致）。

**回滚点**：

- 保留 feature flag，允许快速回到旧 stage 兼容路径（短期）。

---

## Phase 2：RenderPrimitiveBatchSystem 拆分

**目标**：把“巨系统”拆为阶段化小系统。

**建议拆分**：

1. `PrimitiveCullSystem`（RenderCollect）
2. `PrimitiveSortSystem`（RenderBatch）
3. `PrimitiveBatchBuildSystem`（RenderBatch）
4. `PrimitiveBatchFinalizeSystem`（RenderBatch）

**迁移步骤**：

1. 先提取纯函数/私有 helper，不改调用。
2. 再创建新系统并通过依赖链接管流程。
3. 最后将旧系统壳化（仅转发）并删除。

**关键依赖**：

- `TransformSystem`、`CameraSystem`、`BoundingBoxUpdateSystem`。
- `RenderFrameCache` 结构需稳定，避免拆分期间反复改 schema。

**验收标准**：

- `renderItems` 数量、可见数量、material batch 数与拆分前一致。
- Draw 输出一致（允许浮点误差，不允许拓扑缺失/重复）。

---

## Phase 3：TextRenderSystem 拆分

**目标**：分离文本业务数据与GPU资源生命周期。

**建议拆分**：

1. `TextCollectSystem`（RenderCollect）
2. `TextLayoutBuildSystem`（RenderBatch）
3. `TextResourceSyncSystem`（RenderBufferUpload 或 RenderPostBeginFrame）
4. `TextDrawSubmitSystem`（保留现有 submit 角色）

**注意事项**：

- `resources_by_font` 的 owner 明确到单系统，防止析构时跨系统释放。
- 变更掩码（change mask）清除时机必须在一次完整提交流水线后。

**验收标准**：

- 字形布局不抖动、不丢字。
- 字体/材质资源无明显泄漏增长。

---

## Phase 4：RenderSystemCore 瘦身

**目标**：让 core 回归帧壳层。

**迁移原则**：

- 从 `RenderSystemCore::BeginFrame` 移除业务同步步骤，改由 phase 系统执行。
- `RenderSystemCore` 仅保留：
  - frame guard、error handling。
  - RT begin/end。
  - submit 结果聚合与状态推进。

**验收标准**：

- `RenderSystemCore` 代码行数明显下降（建议下降 30% 以上）。
- ECS phase 序列完整可替代原内嵌逻辑。

---

## Phase 5：统一注册与依赖声明

**目标**：让系统注册即文档化。

**任务**：

1. 统一 `RegisterDefaultEcsSystems` 中的注册顺序与依赖声明。
2. 增加 phase 审计脚本/检查（至少开发态日志检查）。
3. 修复/强化依赖：防止“靠注册顺序偶然正确”。

**验收标准**：

- 禁止出现缺依赖仍正确运行的“隐式成功”。
- phase 变更可被日志/检查第一时间发现。

---

## Phase 6：收尾与文档交付

**目标**：形成可维护规范。

**任务**：

1. 更新渲染架构文档与 README 索引。
2. 输出迁移对照表（旧系统 → 新系统）。
3. 归档性能对比报告与回归结论。

**验收标准**：

- 新成员可按文档快速理解“Pipeline 与 ECS 边界”。
- 关键系统具备明确 owner 与变更准入规则。

---

## 8. 目标执行序列（建议版）

## 8.1 Render 前序（无 draw）

1. `SwapchainNextImageSystem` (`RenderSwapchainNextImage`)
2. `RenderTargetSystem` (`RenderPreBeginFrame`)
3. `EnvironmentSystem` (`RenderPreBeginFrame`)
4. `QuadResourcePrepareSystem` (`RenderPreBeginFrame`)
5. `QuadMaterialBindingSystem` (`RenderPreBeginFrame`)
6. `RenderBufferCommitSystem` (`RenderBufferCommit`)
7. `RenderBufferUploadSystem` (`RenderBufferUpload`)
8. `CameraSystem` descriptor bind（建议逐步迁移成独立 render-phase system）

## 8.2 Render 主体

1. `RenderPrimitiveCollectSystem` (`RenderCollect`)
2. `TextCollectSystem`（拆分后）
3. `PrimitiveCull/Sort/BatchBuild/Finalize`（拆分后）
4. `TextLayoutBuildSystem`（拆分后）
5. `RenderPrimitiveSubmitSystem` (`RenderDrawSubmit`)
6. `TextRenderSubmitSystem` (`RenderDrawSubmit`)
7. `LineRenderSystem` (`RenderPostProcess`)

## 8.3 Render 收尾

1. `SwapchainSubmitSystem` (`RenderSubmit`)

---

## 9. 风险清单与对策

### 风险 R1：渲染顺序回归（文本/线框覆盖异常）

- 对策：固化 draw 顺序快照（primitive→text→line），每阶段做截图对比。

### 风险 R2：资源生命周期错配（提前释放或泄漏）

- 对策：为 text/quad 资源 owner 建立明确释放责任，并在 Shutdown 路径加入统计日志。

### 风险 R3：phase 依赖断裂导致隐性数据竞争

- 对策：系统间依赖显式化（`AddDependency`），并对关键系统做“依赖缺失报警”。

### 风险 R4：窗口 resize / swapchain 重建时状态不同步

- 对策：把 render target 同步入口唯一化（`RenderTargetSystem`），禁止多点各自刷新。

---

## 10. 回滚与灰度策略

1. 每个 Phase 以独立 PR 落地，不跨阶段混改。
2. 保留短期开关（例如 `USE_LEGACY_RENDER_STAGE_ECS_PATH`）用于紧急回退。
3. 任一阶段若出现以下情况立即回滚：
   - 连续崩溃。
   - 帧时退化 >15%。
   - draw 输出出现稳定性错误（丢物体/错层）。

---

## 11. 验证矩阵（每阶段必须跑）

## 11.1 功能验证

- Mesh 正常渲染。
- 文本更新（动态改字）正常。
- Line overlay 正常叠加。
- Resize 后首帧无黑屏/崩溃。

## 11.2 性能验证

- 平均帧时、P95 帧时。
- RenderCollect/Bath/Submit 各阶段耗时。
- 批次数、draw call 数。

## 11.3 稳定性验证

- 连续运行 10 分钟无崩溃。
- 热加载/资源切换无明显泄漏增长。

---

## 12. 代码改造任务分解（可直接建 issue）

1. **Issue A**：Render 入口与边界清理（Phase 1）。
2. **Issue B**：PrimitiveBatch 拆分（Phase 2）。
3. **Issue C**：TextRender 拆分（Phase 3）。
4. **Issue D**：RenderSystemCore 瘦身（Phase 4）。
5. **Issue E**：系统注册与依赖审计（Phase 5）。
6. **Issue F**：文档与性能报告收尾（Phase 6）。

每个 issue 统一模板：

- 背景
- 变更范围（文件清单）
- 不变约束
- 验收标准
- 回滚方案

---

## 13. 里程碑建议（示例）

- **M1（1~2天）**：完成 Phase 0 + Phase 1。
- **M2（2~4天）**：完成 Phase 2（Primitive 拆分）。
- **M3（2~3天）**：完成 Phase 3（Text 拆分）。
- **M4（1~2天）**：完成 Phase 4 + Phase 5。
- **M5（1天）**：完成 Phase 6 文档收尾与验收报告。

> 总计建议：7~12 个工作日（按当前代码规模与回归成本估算）。

---

## 14. 决策待办（必须先定）

1. `RenderFrameSystem` 是否正式废弃（推荐：是）。
2. `RenderStagePipeline` 是否作为“多 pass 编排 API”保留（推荐：是）。
3. Text/Line 最终叠加顺序是否固定为 Primitive → Text → Line（推荐：是，除非业务另有要求）。

---

## 15. 最终交付物清单

1. 架构边界文档（本文件）。
2. 迁移对照表（旧系统→新系统）。
3. 每阶段回归报告（截图+计数+帧时）。
4. `RegisterDefaultEcsSystems` 最终顺序说明。
5. 渲染 phase 审计清单（自动或半自动）。

---

## 附录 A：边界判定规则（执行时快速判断）

某段逻辑若满足以下任一条件，则应归到 ECS，而不是 RenderStage：

- 需要访问 Entity/Component。
- 需要调用 ECS 系统（`GetSystem<T>`）。
- 依赖 `ExecutionPhase` 排序语义。

某段逻辑若仅涉及以下内容，则可归到 RenderStage：

- RenderTarget / RenderPass / CommandBuffer 图形编排。
- 与业务无关的 pass 结构复用。

---

## 附录 B：重构完成判定（Definition of Done）

- 单一主入口已确定并文档化。
- Pipeline 与 ECS 无职责交叉。
- 两个超大系统完成拆分并通过回归。
- `RenderSystemCore` 仅承担帧壳层职责。
- 性能与稳定性达到基线要求。
