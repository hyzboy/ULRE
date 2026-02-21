#include<hgl/ecs/core/RenderGraph.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/SubWorldComponent.h>
#include<hgl/ecs/systems/render/RenderSystemCore.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/log/Log.h>

namespace hgl
{
    namespace ecs
    {
        // ========== RenderGraph Implementations ==========

        size_t RenderGraph::GetEnabledPassCount() const
        {
            size_t count = 0;
            for (const auto& pass : passes)
            {
                if (pass.enabled)
                    count++;
            }
            return count;
        }

        void ECSContext::Render(float deltaTime, const RenderGraph& graph)
        {
            Render(deltaTime, graph, nullptr);
        }

        void ECSContext::Render(float deltaTime, const RenderGraph& graph, const std::function<void(float)>& pre_render)
        {
            if (!active)
                return;

            // Canonical frame entry with RenderGraph
            LogInfo("[ECS RENDER] ===== Frame Start (RenderGraph with %zu passes) =====", graph.GetEnabledPassCount());

            if (!render_core)
            {
                render_core = std::make_unique<RenderSystemCore>(this);
                if (!render_core->Initialize())
                {
                    render_core.reset();
                    return;
                }
            }

            // Standard frame setup (swapchain acquire, setup)
            if (auto *rt = GetRenderTarget())
            {
                LogInfo("[ECS RENDER] Calling WaitFence");
                if (!rt->WaitFence())
                {
                    LogWarning("[ECS RENDER] WaitFence FAILED");
                    return;
                }
            }

            LogInfo("[ECS RENDER] Calling AcquireSwapchainImage");
            if (!AcquireSwapchainImage(0.0f))
            {
                LogWarning("[ECS RENDER] AcquireSwapchainImage FAILED");
                return;
            }

            LogInfo("[ECS RENDER] Calling RenderPreBeginFrame");
            RenderPreBeginFrame(0.0f);

            SyncRenderTargetViewport();

            render_core->SetClearColor(clear_color);

            LogInfo("[ECS RENDER] Calling BeginFrame");
            if (!render_core->BeginFrame())
            {
                LogWarning("[ECS RENDER] BeginFrame FAILED");
                return;
            }

            SetCurrentRenderCmd(render_core->GetRenderCmd());
            PrepareRenderPassSetup(render_core->GetSwapchainImageIndex(), 0.0f);

            LogInfo("[ECS RENDER] Calling BeginRenderPass");
            if (!render_core->BeginRenderPass())
            {
                LogWarning("[ECS RENDER] BeginRenderPass FAILED");
                render_core->EndFrame();
                SetCurrentRenderCmd(nullptr);
                return;
            }

            if (pre_render)
                pre_render(deltaTime);

            // Execute each render pass in the graph
            for (size_t pass_idx = 0; pass_idx < graph.passes.size(); ++pass_idx)
            {
                const auto& pass = graph.passes[pass_idx];

                if (!pass.enabled)
                {
                    LogDebug("[ECS RENDER] Skipping disabled pass %zu (phases %d-%d)", 
                             pass_idx, static_cast<int>(pass.startPhase), static_cast<int>(pass.endPhase));
                    continue;
                }

                LogInfo("[ECS RENDER] Executing pass %zu (phases %d-%d)", 
                        pass_idx, static_cast<int>(pass.startPhase), static_cast<int>(pass.endPhase));

                // Invoke before-pass callback
                if (pass.onBeforePass)
                {
                    LogDebug("[ECS RENDER] Invoking onBeforePass for pass %zu", pass_idx);
                    pass.onBeforePass(*this, pass);
                }

                // Step 1: Update() pass — collect, cull, sort, batch (CPU data preparation)
                {
                    HGL_CAPTURE_SCOPE();
                    LogDebug("[ECS RENDER] Update phase range %d to %d", 
                            static_cast<int>(pass.startPhase), static_cast<int>(pass.endPhase));
                    RunRenderUpdatesRange(pass.startPhase, pass.endPhase, deltaTime);
                }

                // Step 2: Submit transform data to GPU before recording draw commands
                {
                    LogDebug("[ECS RENDER] Submitting transform updates");
                    if (auto transform_system = GetSystem<TransformSystem>())
                        transform_system->SubmitTransformUpdates();
                }

                // Step 3: Render() pass — record GPU draw commands
                {
                    HGL_CAPTURE_SCOPE();
                    LogDebug("[ECS RENDER] Render phase range %d to %d", 
                            static_cast<int>(pass.startPhase), static_cast<int>(pass.endPhase));
                    RunRenderSystemsInRange(pass.startPhase, pass.endPhase, deltaTime);
                }

                // Invoke after-pass callback
                if (pass.onAfterPass)
                {
                    LogDebug("[ECS RENDER] Invoking onAfterPass for pass %zu", pass_idx);
                    pass.onAfterPass(*this, pass);
                }

                LogDebug("[ECS RENDER] Completed pass %zu", pass_idx);
            }

            // Render sub-worlds attached via SubWorldComponent
            if (sub_world_auto_update)
            {
                std::vector<std::shared_ptr<SubWorldComponent>> sub_worlds;
                GetComponents(sub_worlds);
                for (const auto& sub_world : sub_worlds)
                {
                    if (sub_world)
                    {
                        LogDebug("[ECS RENDER] Rendering sub-world: %s", sub_world->GetName().c_str());
                        sub_world->RenderSubWorld(render_core->GetRenderCmd(), deltaTime);
                    }
                }
            }

            LogInfo("[ECS RENDER] Calling EndFrame");
            render_core->EndFrame();

            SetCurrentRenderCmd(nullptr);

            LogInfo("[ECS RENDER] Calling SubmitFrameToRenderTarget");
            if (!SubmitFrameToRenderTarget(0.0f))
                LogError("[ECS RENDER] SubmitFrameToRenderTarget FAILED");

            if (wait_idle_enabled)
            {
                LogInfo("[ECS RENDER] Calling WaitIdle");
                if (auto *device = GetGPUDevice())
                    device->WaitIdle();
            }

            LogInfo("[ECS RENDER] ===== Frame End (RenderGraph) =====");
        }

        RenderGraph CreateDefaultLinearGraph()
        {
            RenderGraph graph;

            // Define the default linear render pipeline as it currently exists
            // This ensures backward compatibility with the existing render flow

            // Pre-frame setup phases (RenderPreBeginFrame, RenderSwapchainNextImage, RenderBeginFrame, etc.)
            // These happen before BeginRenderPass starts
            
            // Main render phases: from first Collect phase to last PostProcess phase
            graph.Add(RenderGraph::Pass(
                ExecutionPhase::RenderCollect_RenderPrimitiveCollectSystem,
                ExecutionPhase::RenderPostProcess_LineRenderSystem,
                nullptr,  // nullptr = use current/swapchain RT
                true      // enabled by default
            ));

            // Note: SwapchainSubmitSystem (phase 28) is handled by SubmitFrameToRenderTarget()
            // after the pass loop — no separate pass needed here.

            return graph;
        }

    }//namespace ecs
}//namespace hgl
