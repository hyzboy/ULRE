#include<hgl/ecs/core/RenderGraph.h>
#include<hgl/ecs/core/SystemGroup.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/systems/render/RenderSystemCore.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/log/Log.h>
#include<algorithm>


DEFINE_LOGGER_MODULE(RenderGraph)

namespace hgl
{
    namespace ecs
    {
        // ========== SystemGroup Initialization ==========

        void InitializeSystemGroups(ECSContext* context)
        {
            auto& registry = SystemGroupRegistry::Get();
            registry.Clear();

            if (!context)
            {
                MLogWarning(RenderGraph, "[RenderGraph] InitializeSystemGroups called with null context");
                return;
            }

            std::vector<std::string> element_types;
            context->GetAllRenderElementTypes(element_types);

            for (const auto& element_type : element_types)
            {
                std::vector<std::shared_ptr<System>> systems;
                context->GetSystemsByElementType(element_type, systems);

                if (systems.empty())
                    continue;

                bool has_phase = false;
                ExecutionPhase start_phase = static_cast<ExecutionPhase>(0);
                ExecutionPhase end_phase = static_cast<ExecutionPhase>(0);

                for (const auto& system : systems)
                {
                    if (!system)
                        continue;

                    const ExecutionPhase phase = system->GetExecutionPhase();
                    if (!has_phase)
                    {
                        start_phase = phase;
                        end_phase = phase;
                        has_phase = true;
                    }
                    else
                    {
                        if (phase < start_phase) start_phase = phase;
                        if (phase > end_phase) end_phase = phase;
                    }
                }

                if (!has_phase)
                    continue;

                registry.RegisterGroup(SystemGroup(element_type, start_phase, end_phase, true));
            }

            MLogInfo(RenderGraph, "[RenderGraph] Initialized %zu system groups", registry.GetAllGroups().size());
            registry.DebugPrint();
        }

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

            // Prepare all sub-worlds BEFORE opening the render pass.
            // This runs each sub-world's Collect → Batch → BufferUpload so that
            // StagedBuffers (e.g. transform_vab) are fully uploaded to GPU before
            // BeginRenderPass — vkCmdCopyBuffer is not allowed inside a render pass.
            if (sub_world_auto_update)
            {
                std::vector<std::shared_ptr<SubWorldComponent>> sub_worlds;
                GetComponents(sub_worlds);
                for (const auto& sw : sub_worlds)
                {
                    if (sw)
                    {
                        LogDebug("[ECS RENDER] PrepareSubWorld: %s", sw->GetName().c_str());
                        sw->PrepareSubWorld(deltaTime);
                    }
                }
            }

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

                // Step 1: optional Update() pass — inside the render pass only dispatch
                // phases from RenderDrawSubmit onward.  Collect/Batch/Upload/FrameSync
                // were already executed by PrepareRenderPassSetup before BeginRenderPass;
                // re-running them here would duplicate CPU work and, for RenderBufferUpload,
                // issue vkCmdCopyBuffer inside a Vulkan render pass (spec violation).
                if (pass.runUpdate)
                {
                    const ExecutionPhase update_min =
                        std::max(pass.startPhase, ExecutionPhase::RenderDrawSubmit);
                    if (update_min <= pass.endPhase)
                    {
                        HGL_CAPTURE_SCOPE();
                        LogDebug("[ECS RENDER] Update phase range %d to %d (clamped from %d)",
                                static_cast<int>(update_min), static_cast<int>(pass.endPhase),
                                static_cast<int>(pass.startPhase));
                        RunRenderUpdatesRange(update_min, pass.endPhase, deltaTime);
                    }
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

            // Draw sub-worlds inside the render pass.
            // Their CPU work (Collect/Batch/Upload) was already done by PrepareSubWorld()
            // above — only GPU draw commands are issued here.
            if (sub_world_auto_update)
            {
                std::vector<std::shared_ptr<SubWorldComponent>> sub_worlds;
                GetComponents(sub_worlds);
                for (const auto& sub_world : sub_worlds)
                {
                    if (sub_world)
                    {
                        LogDebug("[ECS RENDER] DrawSubWorld: %s", sub_world->GetName().c_str());
                        sub_world->DrawSubWorld(render_core->GetRenderCmd(), deltaTime);
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

            std::vector<const Entity*> entities;
            context->GetAllEntities(entities);

            for (const Entity* entity : entities)
            {
                if (!entity)
                    continue;

                std::vector<std::shared_ptr<Component>> components;
                entity->GetAllComponents(components);
                for (const auto& component : components)
                {
                    if (!component)
                        continue;

                    const char* group_name = component->GetSystemGroupName();
                    if (group_name && *group_name)
                    {
                        stats.active_render_groups.emplace(group_name);
                    }
                }
            }

            MLogDebug(RenderGraph,"[RenderGraph] Scene stats: detected %zu active render groups",
                     stats.active_render_groups.size());
            for (const auto& group_name : stats.active_render_groups)
            {
                MLogDebug(RenderGraph,"[RenderGraph]   active group: %s", group_name.c_str());
            }

            return stats;
        }

        RenderGraph CreateAdaptiveRenderGraph(ECSContext* context)
        {
            RenderGraph graph;
            SceneStats stats = GatherSceneStats(context);

            MLogDebug(RenderGraph,"[RenderGraph] Adaptive: detected %zu active groups",
                     stats.active_render_groups.size());

            auto& registry = SystemGroupRegistry::Get();
            InitializeSystemGroups(context);

            // Enable/disable groups based on detected component-driven groups
            const auto all_groups = registry.GetAllGroups();
            for (const auto& group : all_groups)
            {
                const bool enabled = stats.HasGroup(group.name);
                registry.SetGroupEnabled(group.name, enabled);

                if (context)
                {
                    context->SetElementTypeSystemsEnabled(group.name, enabled);
                }

                MLogDebug(RenderGraph,"[RenderGraph] Group '%s': %s",
                         group.name.c_str(), enabled ? "ENABLED" : "DISABLED");
            }

            // === Build passes from enabled groups ===
            // Each enabled group becomes a pass in the graph
            auto enabled_groups = registry.GetEnabledGroups();
            MLogDebug(RenderGraph,"[RenderGraph] Adding %zu enabled system groups as passes", enabled_groups.size());

            for (const auto& group : enabled_groups)
            {
                MLogDebug(RenderGraph,"[RenderGraph] Adding pass for group '%s' (phases %d-%d)",
                         group.name.c_str(),
                         static_cast<int>(group.startPhase),
                         static_cast<int>(group.endPhase));

                graph.Add(RenderGraph::Pass(
                    group.startPhase,
                    group.endPhase,
                    nullptr,           // use current render target
                    true,              // pass enabled
                    true,              // run Update()
                    true,              // submit transforms
                    true               // run Render()
                ));
            }

            // Fully data-driven:
            // - Components declare their group via Component::GetSystemGroupName()
            // - Systems declare their group via System::SetRenderElementType()
            // - RenderGraph builds passes from the name mapping at runtime

            return graph;
        }

        RenderGraph CreateDefaultLinearGraph(ECSContext* context)
        {
            RenderGraph graph;

            auto& registry = SystemGroupRegistry::Get();
            InitializeSystemGroups(context);

            // Enable all registered groups for default graph (full compatibility)
            const auto all_groups = registry.GetAllGroups();
            for (const auto& group : all_groups)
            {
                registry.SetGroupEnabled(group.name, true);
                if (context)
                {
                    context->SetElementTypeSystemsEnabled(group.name, true);
                }
            }

            MLogDebug(RenderGraph,"[RenderGraph] CreateDefaultLinearGraph: All groups ENABLED");

            // Build passes from all enabled groups
            auto enabled_groups = registry.GetEnabledGroups();
            for (const auto& group : enabled_groups)
            {
                MLogDebug(RenderGraph,"[RenderGraph] Adding pass for group '%s' (phases %d-%d)",
                         group.name.c_str(),
                         static_cast<int>(group.startPhase),
                         static_cast<int>(group.endPhase));

                graph.Add(RenderGraph::Pass(
                    group.startPhase,
                    group.endPhase,
                    nullptr,
                    true,  // enabled
                    true,  // run Update()
                    true,  // submit transforms
                    true   // run Render()
                ));
            }

            return graph;
        }

    }//namespace ecs
}//namespace hgl
