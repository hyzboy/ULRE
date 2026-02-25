# ECS System / ExecutionPhase 复审报告（2026-02-21）

## 1) 结论总览

当前系统已经从“priority 依赖”切到“phase + dependency + 注册顺序”，方向正确。  
主要问题已从“排序机制”转移到“命名语义与职责边界”：

- 有些系统名不能准确表达真实职责（例如 `RenderTargetSystem` 更像上下文同步器）。
- 有些系统承担了多段职责（CPU 计算 + GPU 编码/提交混在一起），导致 phase 语义被稀释。
- 少数系统元数据不完整（默认 phase/type）。

---

## 2) 命名/Phase 不匹配清单（当前 -> 建议）

### A. 轻量重命名（不改行为）

1. `RenderTargetSystem`  
   - 当前职责：同步 render target 到 render context，并回写 camera viewport/context。  
   - 建议名：`RenderContextSyncSystem`  
   - 建议 phase：`RenderPreBeginFrame`（保持）

2. `SwapchainNextImageSystem`  
   - 当前职责：acquire next image。  
   - 建议名：`SwapchainAcquireSystem`  
   - 建议 phase：`RenderSwapchainNextImage`（保持）

3. `SwapchainSubmitSystem`  
   - 当前职责：submit/present。  
   - 建议名：`SwapchainPresentSystem`  
   - 建议 phase：`RenderSubmit`（保持）

4. `RenderPrimitiveSubmitSystem`  
   - 当前职责：批次绘制提交。  
   - 建议名：`PrimitiveDrawSubmitSystem`  
   - 建议 phase：`RenderDrawSubmit`（保持）

5. `TextRenderSubmitSystem`  
   - 当前职责：实际 draw，不是后处理。  
   - 建议名：`TextDrawSubmitSystem`  
   - 建议 phase：建议迁移到 `RenderDrawSubmit`（当前在 `RenderPostProcess`，语义偏离）

---

### B. 元数据修正（应优先处理）

1. `FacingTransformSystem`（transform 目录）
   - 问题：未显式设置 phase/type，容易落在默认 phase。  
   - 建议：`SetExecutionOrder(ExecutionPhase::TickPostCamera)`，type 至少设为 Transform。

2. `QuadRenderSystem`
   - 问题：未显式 phase/type。  
   - 建议：若职责是资源准备，放 `RenderPreBeginFrame`；若是每帧绘制，放 `RenderDrawSubmit`。

3. `VisibilitySystem`
   - 问题：SystemType 语义过弱（Unknown）。  
   - 建议：设为 `BoundingBox` 或新增 `Visibility` 类型（若你愿意扩 enum）。

---

## 3) 可拆解系统（明确边界）

### 3.1 `RenderPrimitiveBatchSystem`（强烈建议拆）

当前混合职责：
- 可见性/裁剪
- 距离排序
- transform index 分配
- material batch 组包
- 间接绘制数据编码/上传

建议拆分为 4 个：
1. `PrimitiveCullSystem`（`RenderCollect`）
2. `PrimitiveSortSystem`（`RenderBatch`）
3. `PrimitiveBatchBuildSystem`（`RenderBatch`）
4. `PrimitiveBatchGpuEncodeSystem`（`RenderBufferUpload`）

依赖链：
- Collect -> Cull -> Sort -> BatchBuild -> GpuEncode -> DrawSubmit

---

### 3.2 `CameraSystem`（建议拆）

当前混合职责：
- 输入驱动控制（模拟）
- camera 矩阵求解
- UBO/descriptor 资源绑定与更新

建议拆分：
1. `CameraControlSystem`（`TickCamera`）
2. `CameraGpuBindingSystem`（`RenderPostBeginFrame`）

---

### 3.3 `TransformSystem`（中等优先）

当前混合职责：
- Tick 中 CPU 变换求解
- Render 路径里 GPU assignment/upload

建议拆分：
1. `TransformUpdateSystem`（`TickTransform`）
2. `TransformUploadSystem`（`RenderBufferUpload`）

---

### 3.4 `TextRenderSystem`（建议拆）

当前混合职责：
- 文本组件收集
- font/material/pipeline 资源管理
- glyph layout 与几何构建

建议拆分：
1. `TextCollectSystem`（`RenderCollect`）
2. `TextBuildSystem`（`RenderBatch`）
3. `TextDrawSubmitSystem`（`RenderDrawSubmit`，由现 TextRenderSubmitSystem 演进）

---

### 3.5 `EnvironmentSystem`（可拆）

当前职责：资源持有 + 每帧 UBO 更新/提交耦合。  
建议拆分：
1. `EnvironmentResourceSystem`（`RenderPreBeginFrame`）
2. `EnvironmentSyncSystem`（`RenderBufferUpload`）

---

## 4) 推荐执行顺序（目标态，明确且稳定）

## Tick
1. `InputSystem` (`TickInput`)
2. `TransformUpdateSystem` (`TickTransform`)
3. `BoundingBoxUpdateSystem` (`TickTransform`, depends Transform)
4. `VisibilitySystem` (`TickTransform`)
5. `CameraControlSystem` (`TickCamera`, depends Input+Transform)
6. `FacingTransformSystem` (`TickPostCamera`, depends Camera)

## Render Pre-frame
7. `SwapchainAcquireSystem` (`RenderSwapchainNextImage`)
8. `RenderContextSyncSystem` (`RenderPreBeginFrame`)
9. `EnvironmentResourceSystem` (`RenderPreBeginFrame`)
10. `RenderBeginFrame`（保留钩子）

## Render Upload Cycles
11. `RenderBufferCommitSystem` (`RenderBufferCommit`)
12. `TransformUploadSystem` (`RenderBufferUpload`)
13. `EnvironmentSyncSystem` (`RenderBufferUpload`)
14. `RenderBufferUploadSystem` (`RenderBufferUpload`, depends Commit)
15. `CameraGpuBindingSystem` (`RenderPostBeginFrame`)

## Render Build / Submit
16. `RenderPrimitiveCollectSystem` (`RenderCollect`)
17. `PrimitiveCullSystem` (`RenderCollect`)
18. `TextCollectSystem` (`RenderCollect`)
19. `PrimitiveSortSystem` (`RenderBatch`)
20. `PrimitiveBatchBuildSystem` (`RenderBatch`)
21. `TextBuildSystem` (`RenderBatch`)
22. `PrimitiveDrawSubmitSystem` (`RenderDrawSubmit`)
23. `TextDrawSubmitSystem` (`RenderDrawSubmit`)
24. `LineRenderSystem` (`RenderPostProcess` 或 Overlay 专用 phase)
25. `SwapchainPresentSystem` (`RenderSubmit`)

---

## 5) 低风险实施步骤（建议）

1. **先做元数据修正**：`FacingTransformSystem`/`QuadRenderSystem` 明确 phase/type。  
2. **只做 rename alias**：先加新类名别名，不马上删旧名。  
3. **先拆 GPU 路径**：Transform/Environment 的 upload/sync 先独立。  
4. **再拆 Batch 大系统**：按 Cull -> Sort -> Build -> Encode 逐步拆。  
5. **最后调 Text/Line 顺序**：统一到 DrawSubmit/Overlay 语义。  

---

## 6) 关键风险

- 文本/线框覆盖顺序回归（layering）。
- 子世界（SubWorld）注册链路与主世界不一致导致顺序分叉。
- 隐式依赖“注册顺序”的系统在拆分后暴露竞态。

建议每一步都加“phase 执行日志 + 帧内耗时 + 可视回归截图”做对照验证。