#include<hgl/ecs/core/RenderGraph.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/SubWorldComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/TextComponent.h>
#include<hgl/ecs/components/BillboardComponent.h>
#include<hgl/ecs/systems/render/RenderSystemCore.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCullSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveSortSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveBatchBuildSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveBatchFinalizeSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveSubmitSystem.h>
#include<hgl/ecs/systems/render/TextCollectSystem.h>
#include<hgl/ecs/systems/render/TextBuildSystem.h>
#include<hgl/ecs/systems/render/TextResourceSyncSystem.h>
#include<hgl/ecs/systems/render/TextRenderSubmitSystem.h>
#include<hgl/ecs/systems/render/LineRenderSystem.h>
#include<hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include<hgl/ecs/systems/render/QuadMaterialBindingSystem.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/log/Log.h>


DEFINE_LOGGER_MODULE(RenderGraph)

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

                // Step 1: optional Update() pass — collect, cull, sort, batch (CPU data preparation)
                if (pass.runUpdate)
                {
                    HGL_CAPTURE_SCOPE();
                    LogDebug("[ECS RENDER] Update phase range %d to %d", 
                            static_cast<int>(pass.startPhase), static_cast<int>(pass.endPhase));
                    RunRenderUpdatesRange(pass.startPhase, pass.endPhase, deltaTime);
                }

                // Step 2: optional transform submit before Render()
                if (pass.submitTransforms)
                {
                    LogDebug("[ECS RENDER] Submitting transform updates");
                    if (auto transform_system = GetSystem<TransformSystem>())
                        transform_system->SubmitTransformUpdates();
                }

                // Step 3: optional Render() pass — record GPU draw commands
                if (pass.runRender)
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

        SceneStats GatherSceneStats(ECSContext* context)
        {
            SceneStats stats;
            if (!context)
                return stats;

            // Check for Primitives (includes Quad through hierarchy)
            std::vector<std::shared_ptr<PrimitiveComponent>> primitives;
            context->GetComponents<PrimitiveComponent>(primitives);
            stats.hasPrimitives = !primitives.empty();

            // Check for Text
            std::vector<std::shared_ptr<TextComponent>> texts;
            context->GetComponents<TextComponent>(texts);
            stats.hasText = !texts.empty();

            // Check for Billboards
            std::vector<std::shared_ptr<BillboardComponent>> billboards;
            context->GetComponents<BillboardComponent>(billboards);
            stats.hasBillboards = !billboards.empty();

            // Note: Line detection would require LineComponent or special marker
            // For now, assume lines always exist (lazy-init in LineRenderSystem)
            stats.hasLines = true;

            // Environment detection could check for specialized environmental components
            // For now, assume environment system always runs
            stats.hasEnvironment = true;

            MLogDebug(RenderGraph,"[RenderGraph] Scene stats: Primitives=%d Text=%d Lines=%d Billboards=%d",
                     stats.hasPrimitives, stats.hasText, stats.hasLines, stats.hasBillboards);

            return stats;
        }

        RenderGraph CreateAdaptiveRenderGraph(ECSContext* context)
        {
            RenderGraph graph;
            SceneStats stats = GatherSceneStats(context);

            MLogDebug(RenderGraph,"[RenderGraph] Adaptive: Primitives=%d Text=%d Lines=%d Billboards=%d",
                     stats.hasPrimitives, stats.hasText, stats.hasLines, stats.hasBillboards);

            // === System Group Management ===
            // Use per-element-type API to enable/disable system groups based on scene content
            context->SetElementTypeSystemsEnabled("Primitive", stats.hasPrimitives);
            context->SetElementTypeSystemsEnabled("Text", stats.hasText);
            context->SetElementTypeSystemsEnabled("Line", stats.hasLines);
            context->SetElementTypeSystemsEnabled("Billboard", stats.hasBillboards);

            if (stats.hasPrimitives) {
                MLogDebug(RenderGraph,"[RenderGraph] Enabling Primitive system group");
            } else {
                MLogDebug(RenderGraph,"[RenderGraph] Disabling Primitive system group (no PrimitiveComponents)");
            }

            if (stats.hasText) {
                MLogDebug(RenderGraph,"[RenderGraph] Enabling Text system group");
            } else {
                MLogDebug(RenderGraph,"[RenderGraph] Disabling Text system group (no TextComponents)");
            }

            if (stats.hasLines) {
                MLogDebug(RenderGraph,"[RenderGraph] Enabling Line system group");
            } else {
                MLogDebug(RenderGraph,"[RenderGraph] Disabling Line system group (no lines)");
            }

            if (stats.hasBillboards) {
                MLogDebug(RenderGraph,"[RenderGraph] Enabling Billboard system group");
            } else {
                MLogDebug(RenderGraph,"[RenderGraph] Disabling Billboard system group (no Billboards)");
            }

            // === Single Pass: All Render Phases (systems will check enabled flag) ===
            // The pass covers the full range; individual systems control execution via SetEnabled()
            MLogDebug(RenderGraph,"[RenderGraph] Adaptive: Adding comprehensive render pass (phases 17-27)");
            graph.Add(RenderGraph::Pass(
                ExecutionPhase::RenderCollect_RenderPrimitiveCollectSystem,
                ExecutionPhase::RenderPostProcess_LineRenderSystem,
                nullptr,
                true,  // enabled
                true,  // run Update()
                true,  // submit transforms
                true   // run Render()
            ));

            // === Future extensibility ===
            // To add Particle, Decal, Terrain systems:
            // 1. Add stats.hasParticles, stats.hasDecals, stats.hasTerrain to SceneStats
            // 2. Add corresponding system group enable/disable blocks here
            // 3. Systems will execute/skip based on SetEnabled() calls

            return graph;
        }




        RenderGraph CreateMainSceneGraph()
        {
            RenderGraph graph;

            // Main scene phases: collect/batch/build/submit primitive+text (line excluded)
            graph.Add(RenderGraph::Pass(
                ExecutionPhase::RenderCollect_RenderPrimitiveCollectSystem,
                ExecutionPhase::RenderDrawSubmit_TextRenderSubmitSystem,
                nullptr,  // nullptr = use current/swapchain RT
                true,     // enabled
                true,     // run Update() pass
                true,     // submit transforms
                true      // run Render() pass
            ));

            return graph;
        }

        RenderGraph CreateMainWithLineOverlayGraph()
        {
            RenderGraph graph;

            // Pass 0: main scene (up to text submit)
            graph.Add(RenderGraph::Pass(
                ExecutionPhase::RenderCollect_RenderPrimitiveCollectSystem,
                ExecutionPhase::RenderDrawSubmit_TextRenderSubmitSystem,
                nullptr,
                true,
                true,
                true,
                true
            ));

            // Pass 1: line overlay render-only
            graph.Add(RenderGraph::Pass(
                ExecutionPhase::RenderPostProcess_LineRenderSystem,
                ExecutionPhase::RenderPostProcess_LineRenderSystem,
                nullptr,
                true,
                false,
                false,
                true
            ));

            return graph;
        }

        RenderGraph CreateLineOnlyGraph()
        {
            RenderGraph graph;

            graph.Add(RenderGraph::Pass(
                ExecutionPhase::RenderPostProcess_LineRenderSystem,
                ExecutionPhase::RenderPostProcess_LineRenderSystem,
                nullptr,
                true,
                false,
                false,
                true
            ));

            return graph;
        }

        RenderGraph CreateDefaultLinearGraph()
        {
            RenderGraph graph;

            // For backward compatibility, default keeps a single combined pass.

            graph.Add(RenderGraph::Pass(
                ExecutionPhase::RenderCollect_RenderPrimitiveCollectSystem,
                ExecutionPhase::RenderPostProcess_LineRenderSystem,
                nullptr,
                true,
                true,
                true,
                true
            ));

            // Note: SwapchainSubmitSystem (phase 28) is handled by SubmitFrameToRenderTarget()
            // after the pass loop — no separate pass needed here.

            return graph;
        }

    }//namespace ecs
}//namespace hgl
