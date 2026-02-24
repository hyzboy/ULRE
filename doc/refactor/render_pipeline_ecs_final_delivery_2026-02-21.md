# Render Pipeline / ECS 重构最终交付（2026-02-21）

## 1. 最终状态

本轮重构已完成 Phase 1~Phase 6 目标，当前渲染链路满足以下条件：

- 单一帧驱动入口：`ECSContext::Render(float)`。
- `RenderSystemCore` 已收敛为帧壳层：负责 frame 命令生命周期与 render pass 边界。
- `RenderStagePipeline` 保留图形编排职责，不再承载 ECS 业务同步。
- Primitive/Text 大系统拆分完成，旧系统已移除（包括 `RenderPrimitiveBatchSystem`、`TextRenderSystem`）。
- 关键同/跨 phase 依赖已显式声明，降低对注册顺序的隐式耦合。

---

## 2. 边界定义（最终版）

### 2.1 Frame Driver（`ECSContext` + `RenderSystemCore`）

- `ECSContext::Render(float)` 负责：
  - Acquire/PreBegin/ViewportSync/pre-pass phase 编排
  - draw 阶段调用
  - submit 流程与错误日志
- `RenderSystemCore` 负责：
  - `BeginFrame()`：建立 frame command
  - `BeginRenderPass()`：开启 render pass
  - `EndFrame()`：结束 render pass + EndRender + 状态收尾

### 2.2 ECS Systems（业务渲染层）

- `RenderCollect`：Primitive/Text collect
- `RenderBatch`：Cull/Sort/BatchBuild/BatchFinalize、TextBuild/Sync
- `RenderDrawSubmit`：Primitive submit、Text submit
- `RenderPostProcess`：Line overlay
- `RenderSubmit`：Swapchain submit

### 2.4 ExecutionPhase（按 system 精细化）

为实现“从外部仅看 Phase 即可定位 system”，`ExecutionPhase` 已细化为按系统命名，示例：

- `RenderPreBeginFrame_RenderTargetSystem`
- `RenderPreBeginFrame_EnvironmentSystem`
- `RenderCollect_RenderPrimitiveCollectSystem`
- `RenderBatch_TextBuildSystem`
- `RenderDrawSubmit_TextRenderSubmitSystem`
- `RenderSubmit_SwapchainSubmitSystem`

同时，`ECSContext` 的阶段执行入口已适配为范围/精确 phase 调度，保持现有执行语义不变。

此外，`System.h` 已补充 phase 命名约束与维护契约，确保后续新增 system 时，外部开发者仍可通过 phase 名直接定位执行位置。

### 2.3 RenderStagePipeline（图形编排层）

- 仅保留 RenderTarget/RenderPass 编排能力。
- 不再直接触发 ECS 业务调用。

---

## 3. 迁移对照表（旧 -> 新）

| 旧模块 | 新模块/形态 | 状态 |
| --- | --- | --- |
| `RenderPrimitiveBatchSystem` | `PrimitiveBatchPipeline` + `RenderPrimitiveCullSystem` + `RenderPrimitiveSortSystem` + `RenderPrimitiveBatchBuildSystem` + `RenderPrimitiveBatchFinalizeSystem` | 已替换并删除旧实现 |
| `TextRenderSystem` | `TextRenderPipeline` + `TextCollectSystem` + `TextBuildSystem` + `TextResourceSyncSystem` + `TextRenderSubmitSystem` | 已替换并删除旧实现 |
| Begin-frame helper（camera/env bind） | `RenderFrameBusinessSyncSystem` (`RenderPostBeginFrame`) | 已系统化 |
| `RenderSystemCore` 中业务编排 | 下沉到 `ECSContext` 编排函数（`AcquireSwapchainImage`/`PrepareRenderPassSetup`/`SubmitFrameToRenderTarget` 等） | 已完成 |

---

## 4. 关键新增/调整系统

- `RenderPrimitiveCullSystem`
- `RenderPrimitiveSortSystem`
- `RenderPrimitiveBatchBuildSystem`
- `RenderPrimitiveBatchFinalizeSystem`
- `TextCollectSystem`
- `TextBuildSystem`
- `TextResourceSyncSystem`
- `RenderFrameBusinessSyncSystem`
- `PrimitiveBatchPipeline`（support）
- `TextRenderPipeline`（support）

---

## 5. 依赖显式化结果

已补充的关键依赖包括：

- `QuadResourcePrepareSystem -> RenderTargetSystem`
- `QuadMaterialBindingSystem -> QuadResourcePrepareSystem`
- `RenderFrameBusinessSyncSystem -> RenderBufferUploadSystem / EnvironmentSystem / RenderTargetSystem`
- `RenderPrimitiveSubmitSystem -> RenderPrimitiveBatchFinalizeSystem / RenderBufferUploadSystem`
- `TextRenderSubmitSystem -> TextResourceSyncSystem / RenderPrimitiveSubmitSystem / RenderBufferUploadSystem`
- `SwapchainSubmitSystem -> RenderPrimitiveSubmitSystem / TextRenderSubmitSystem / LineRenderSystem`

---

## 6. 回归与验收结论

本轮重构期间已完成并验证：

- 编译通过（用户本地已确认）
- 运行正常（用户本地已确认）
- 历史退出崩溃问题（`TextRenderPipeline` 析构访问失效 `GraphicsContext`）已修复并验证

当前结论：

- 功能回归：通过
- 稳定性回归（启动/渲染/退出）：通过
- 架构目标达成：通过（边界清晰、职责收敛、依赖显式）

---

## 7. 残留事项（可选）

以下为可选增强项，不影响本次交付通过：

1. 增加自动化 phase 审计脚本（输出拓扑与潜在循环）
2. 补充性能基线报告（平均帧时/P95/draw call 数）
3. 清理少量历史注释中对旧系统名称的描述

---

## 8. 交付文件索引

- 重构计划与过程记录：`doc/refactor/render_pipeline_ecs_refactor_plan_2026-02-21.md`
- 本最终交付文档：`doc/refactor/render_pipeline_ecs_final_delivery_2026-02-21.md`
