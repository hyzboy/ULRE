# ECS 渲染链条分析

> 基于 `ECSContext::current_render_cmd` 追踪完整的 Vulkan 渲染调用链
>
> 文件来源：`inc/hgl/ecs/core/Context.h`, `src/ecs/core/Context.cpp`, `src/ecs/core/RenderGraph.cpp`, `src/ecs/systems/render/RenderSystemCore.cpp`, `src/Vulkan/VKCommandBufferRender.cpp`, `src/ecs/support/PipelineMaterialRenderer.cpp`

---

## 1. 关键数据结构

| 字段 | 类型 | 说明 |
|------|------|------|
| `current_render_cmd` | `hgl::graph::RenderCmdBuffer*` | 当前帧渲染命令缓冲区，仅在 `Render()` 执行期间有效 |
| `render_core` | `std::unique_ptr<RenderSystemCore>` | 封装帧生命周期：BeginFrame / BeginRenderPass / EndFrame |
| `render_target` | `hgl::graph::IRenderTarget*` | Swapchain 渲染目标，提供 `BeginRender()` / `EndRender()` |
| `render_pipelines` | `unordered_map<string, RenderPipelineBase>` | 各 SystemGroup 的渲染管线（Primitive / Line / Text …） |

---

## 2. ExecutionPhase 阶段表

```cpp
enum class ExecutionPhase
{
    // ── Tick (逻辑帧) ─────────────────────────────────────────
    TickInput,              // 用户输入
    TickTransform,          // 变换、边界盒、可见性
    TickCamera,             // 相机矩阵
    TickPostCamera,         // Billboard / Facing 变换

    // ── Render 准备（命令缓冲区尚未打开）──────────────────────
    RenderSwapchainNextImage,   // 获取 Swapchain 图像
    RenderPreBeginFrame,        // 每帧环境 / 视口同步
    RenderResourceSetup,        // 懒初始化 GPU 资源
    RenderMaterialBind,         // 每实体材质 / 纹理绑定

    // ── 预渲染 CPU 工作（命令缓冲区已打开，在 RenderPass 外）──
    RenderBeginFrame,       // 打开命令缓冲区，写帧 UBO
    RenderCollect,          // 收集 / 剔除可见组件
    RenderBatch,            // 构建批次，写 VAB（StagedBuffer）
    RenderBufferCommit,     // 完成 CPU staged 写入
    RenderBufferUpload,     // GPU 传输：vkCmdCopyBuffer
    RenderFrameSync,        // 上传后同步 UBO / Descriptor

    // ── RenderPass 内部 ───────────────────────────────────────
    RenderDrawSubmit,       // 录制绘制命令
    RenderPostProcess,      // 后处理
    RenderDebug,            // 调试叠加层

    // ── Pass 后 ───────────────────────────────────────────────
    RenderStat,             // 统计系统
    RenderSubmit            // Present 到 Swapchain
};
```

---

## 3. 完整帧渲染调用链（主路径 `Render(float deltaTime, const RenderGraph&)`）

```
ECSContext::Render(float deltaTime)
│
├─ [自适应模式] CreateAdaptiveRenderGraph()  或  CreateDefaultLinearGraph()
│
└─ ECSContext::Render(float deltaTime, const RenderGraph& graph)
   │
   ├─ 1. render_target->WaitFence()                          ← 等待上一帧 GPU 完成
   │
   ├─ 2. AcquireSwapchainImage()
   │       └─ RunRenderPhaseUpdates(RenderSwapchainNextImage) ← SwapchainNextImageSystem
   │
   ├─ 3. RenderPreBeginFrame()
   │       ├─ RunRenderPhaseUpdates(RenderPreBeginFrame)      ← EnvironmentSystem, RenderTargetSystem
   │       ├─ RunRenderPhaseUpdates(RenderResourceSetup)      ← QuadResourcePrepareSystem
   │       └─ RunRenderPhaseUpdates(RenderMaterialBind)       ← QuadMaterialBindingSystem
   │
   ├─ 4. SyncRenderTargetViewport()
   │
   ├─ 5. RenderSystemCore::BeginFrame()
   │       └─ render_cmd = render_target->BeginRender()       ← 打开 VkCommandBuffer
   │
   ├─ 6. SetCurrentRenderCmd(render_core->GetRenderCmd())     ← current_render_cmd 设置
   │
   ├─ 7. PrepareRenderPassSetup(swapchainImageIndex)
   │       ├─ SetFrameIndex()
   │       ├─ RenderBeginFrame()    → RunPhase(RenderBeginFrame)   ← 帧 UBO 写入
   │       ├─ RunPhase(RenderCollect)                               ← 收集可见对象
   │       ├─ RunPhase(RenderBatch)                                 ← 构建 VAB / DrawBatch
   │       ├─ RenderBufferCommit()  → RunPhase(RenderBufferCommit) ← finalize StagedBuffer
   │       ├─ RenderBufferUpload()  → RunPhase(RenderBufferUpload) ← vkCmdCopyBuffer（GPU传输）
   │       └─ RenderFrameSync()     → RunPhase(RenderFrameSync)    ← RenderDescriptorBindingSystem
   │                                                                   同步 Camera/Sky UBO，绑定 DescriptorBinding
   │
   ├─ 8. [SubWorld] PrepareSubWorld() × N                     ← 递归 Collect→Batch→Upload
   │
   ├─ 9. RenderSystemCore::BeginRenderPass()
   │       ├─ render_cmd->SetClearColor()
   │       └─ render_cmd->BeginRenderPass()
   │               ├─ vkCmdBeginRenderPass(...)               ◄◄◄ RENDER PASS 开始
   │               ├─ vkCmdSetViewport(...)
   │               └─ vkCmdSetScissor(...)
   │
   ├─ 10. [可选] pre_render(deltaTime) 回调
   │
   ├─ 11. 遍历 RenderGraph::passes[]
   │       └─ 每个 pass（默认包含 RenderDrawSubmit → RenderStat）:
   │           ├─ onBeforePass 回调
   │           ├─ RunRenderUpdatesRange(RenderDrawSubmit, endPhase)  ← Update() 阶段（跳过Collect/Batch）
   │           ├─ TransformSystem::SubmitTransformUpdates()
   │           ├─ RunRenderSystemsInRange(startPhase, endPhase)      ← 调用各 System::Render(cmd, dt)
   │           │    │
   │           │    └─▶ PrimitiveRenderSystem::Render(cmd, dt)       ← RenderDrawSubmit 阶段
   │           │            └─ RenderPipelineDrawSystem::Render()
   │           │                    └─ PrimitiveRenderSystem::OnRender(pipeline, cmd)
   │           │                            └─ 遍历 RenderFrameCache::materialBatches
   │           │                                └─ PipelineMaterialRenderer::Render(cmd, batches, ...)
   │           │                                    ├─ cmd->BindPipeline(pipeline)        ← vkCmdBindPipeline
   │           │                                    ├─ cmd->BindDescriptorSets(material)  ← vkCmdBindDescriptorSets
   │           │                                    └─ 遍历 DrawBatches:
   │           │                                        ├─ [几何切换时] cmd->BindVAB()     ← vkCmdBindVertexBuffers
   │           │                                        ├─ [有IBO时]   cmd->BindIBO()      ← vkCmdBindIndexBuffer
   │           │                                        └─ cmd->Draw()
   │           │                                            ├─ [有IBO] vkCmdDrawIndexed()  ◄◄◄ DRAW CALL
   │           │                                            └─ [无IBO] vkCmdDraw()         ◄◄◄ DRAW CALL
   │           │                                           （间接绘制: vkCmdDrawIndexedIndirect / vkCmdDrawIndirect）
   │           │
   │           └─ onAfterPass 回调
   │
   ├─ 12. [SubWorld] DrawSubWorld(cmd, dt) × N               ← 仅 GPU draw，CPU已在步骤8完成
   │
   ├─ 13. RenderSystemCore::EndFrame()
   │       ├─ render_cmd->EndRenderPass()
   │       │       └─ vkCmdEndRenderPass(...)                 ◄◄◄ RENDER PASS 结束
   │       └─ render_target->EndRender()                      ← 结束命令缓冲区录制
   │
   ├─ 14. SetCurrentRenderCmd(nullptr)                        ← current_render_cmd 清空
   │
   └─ 15. SubmitFrameToRenderTarget()
           ├─ RenderSubmit() → RunPhase(RenderSubmit)         ← SwapchainSubmitSystem
           └─ render_target->Submit()                         ← vkQueueSubmit + vkQueuePresentKHR
```

---

## 4. 核心 Vulkan 指令位置速查

| Vulkan 指令 | 调用位置 | 触发路径 |
|---|---|---|
| `vkCmdBeginRenderPass` | `VKCommandBufferRender.cpp` `RenderCmdBuffer::BeginRenderPass()` | `RenderSystemCore::BeginRenderPass()` → 步骤9 |
| `vkCmdSetViewport` | 同上，`BeginRenderPass()` 内 | 同上 |
| `vkCmdSetScissor` | 同上，`BeginRenderPass()` 内 | 同上 |
| `vkCmdEndRenderPass` | `VKCommandBufferRender.cpp` `RenderCmdBuffer::EndRenderPass()` | `RenderSystemCore::EndFrame()` → 步骤13 |
| `vkCmdCopyBuffer` | `RenderBufferUploadSystem::Update()` | `PrepareRenderPassSetup()` → 步骤7，**RenderPass 外** |
| `vkCmdBindPipeline` | `RenderCmdBuffer::BindPipeline()` | `PipelineMaterialRenderer::Render()` → 步骤11 |
| `vkCmdBindDescriptorSets` | `VKCommandBufferRender.cpp` `RenderCmdBuffer::BindDescriptorSets(Material*)` | `PipelineMaterialRenderer::Render()` → 步骤11 |
| `vkCmdBindVertexBuffers` | `VKCommandBufferRender.cpp` `RenderCmdBuffer::BindVAB()` (via `BindDataBuffer`) | `PipelineMaterialRenderer::Draw()` → 几何切换时 |
| `vkCmdBindIndexBuffer` | `VKCommandBufferRender.cpp` `RenderCmdBuffer::BindIBO()` | `PipelineMaterialRenderer::Draw()` → 有 IBO 时 |
| `vkCmdDrawIndexed` | `VKCommandBufferRender.cpp` `RenderCmdBuffer::Draw()` | `PipelineMaterialRenderer::Draw()` → 有 IBO 时 |
| `vkCmdDraw` | `VKCommandBufferRender.cpp` `RenderCmdBuffer::Draw()` | `PipelineMaterialRenderer::Draw()` → 无 IBO 时 |
| `vkCmdDrawIndexedIndirect` | `VKCommandBufferRender.cpp` `RenderCmdBuffer::DrawIndexedIndirect()` | `PipelineMaterialRenderer::ProcIndirectRender()` |

---

## 5. `current_render_cmd` 生命周期

```
BeginFrame()                 → render_target->BeginRender() 返回 RenderCmdBuffer*
SetCurrentRenderCmd(cmd)     → current_render_cmd = cmd         ← 有效开始
│
│  PrepareRenderPassSetup()   RenderBufferUploadSystem::Update() 使用 current_render_cmd
│                             发起 vkCmdCopyBuffer（buffer upload，RenderPass 外）
│
│  RenderDescriptorBindingSystem::Render() 使用 context->GetCurrentRenderCmd()
│                             绑定 Camera / Sky / RenderTarget DescriptorBinding
│
│  BeginRenderPass()          进入 Vulkan RenderPass
│  System::Render(cmd, dt)    各渲染系统通过参数 cmd 录制 draw 指令
│
EndFrame()                   → EndRenderPass()
SetCurrentRenderCmd(nullptr) → current_render_cmd = nullptr      ← 有效结束
```

---

## 6. 替代入口点

| 入口函数 | 使用场景 |
|---|---|
| `Render(RenderCmdBuffer*, float)` | 外部已打开命令缓冲区，仅执行 ECS 渲染系统 |
| `RenderDrawOnly(RenderCmdBuffer*, float)` | `PrepareRenderPassSetup()` 已在 Pass 外执行，仅录制 draw 命令 |
| `Render(float, const RenderGraph&)` | **推荐主路径**，完整帧生命周期由 ECS 驱动 |
| `SubWorldComponent::PrepareSubWorld()` | Sub-World 的 CPU 预处理（Collect/Batch/Upload），在父 World BeginRenderPass 前调用 |
| `SubWorldComponent::DrawSubWorld()` | Sub-World 的 GPU draw，在父 World RenderPass 内调用 |

---

## 7. 关键系统注册阶段一览

| System | ExecutionPhase | 职责 |
|---|---|---|
| `SwapchainNextImageSystem` | `RenderSwapchainNextImage` | 获取 Swapchain 下一张图像 |
| `EnvironmentSystem` | `RenderPreBeginFrame` | 更新天空 / 环境 UBO |
| `RenderTargetSystem` | `RenderPreBeginFrame` | 同步视口到 CameraSystem |
| `QuadResourcePrepareSystem` | `RenderResourceSetup` | 懒初始化 Quad GPU 资源 |
| `QuadMaterialBindingSystem` | `RenderMaterialBind` | 绑定 Quad 材质纹理 |
| `RenderPrimitiveCollectSystem` | `RenderCollect` | 收集可见 Primitive 组件，写入 RenderFrameCache；调用 `MaterialCache::BeginFrame()` 然后对每个 Primitive 走 L1→L2→`ResolveMI`→`BindMaterialSlot` 两级缓存路径 |
| `TextSyncSystem` | `RenderBatch` | 同步文字顶点数据 |
| `RenderBufferUploadSystem` | `RenderBufferUpload` | 遍历 GPUBufferRegistry，执行 vkCmdCopyBuffer |
| `RenderDescriptorBindingSystem` | `RenderFrameSync` | 绑定 Camera/Sky/RenderTarget Descriptor |
| `RenderFrameBusinessSyncSystem` | `RenderFrameSync` | 业务逻辑帧同步 |
| `PrimitiveRenderSystem` | `RenderDrawSubmit` | 绘制 Primitive 批次（VAB/IBO/Draw） |
| `TextRenderSystem` | `RenderDrawSubmit` | 绘制文字 |
| `LineStatsSystem` | `RenderStat` | 统计 Line 渲染数据 |
| `SwapchainSubmitSystem` | `RenderSubmit` | vkQueueSubmit + Present |

---

*生成时间：2026-03-09*
