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
        // ========== SystemGroup Registration ==========

        void EnsureSystemGroupsRegistered(ECSContext* context)
        {
            auto& registry = SystemGroupRegistry::Get();

            if (!context)
            {
                MLogWarning(RenderGraph, "[RenderGraph] EnsureSystemGroupsRegistered called with null context");
                return;
            }

            // Idempotent: register only missing groups. The installer path
            // (DefaultSystems) registers systems/pipelines; this fills in the
            // group metadata (name -> phase range) once, so per-frame graph
            // rebuilds don't Clear + rescan. New element types appearing at
            // runtime (component-driven installer) still get picked up.
            std::vector<std::string> element_types;
            context->GetAllRenderElementTypes(element_types);

            size_t added = 0;
            for (const auto& element_type : element_types)
            {
                if (registry.GetGroup(element_type))
                    continue;

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

                registry.RegisterGroup(SystemGroup(element_type, start_phase, end_phase));
                ++added;
            }

            if (added)
            {
                MLogInfo(RenderGraph, "[RenderGraph] Registered %zu system groups (total %zu)",
                         added, registry.GetAllGroups().size());
                registry.DebugPrint();
            }
        }

        // ========== RenderGraph Implementations ==========

        void ECSContext::ExecuteRenderGraphPasses(const RenderGraph& graph,
                                                  float deltaTime,
                                                  const std::function<void(float)>& pre_render)
        {
            if (pre_render)
                pre_render(deltaTime);

            for (size_t pass_idx = 0; pass_idx < graph.passes.size(); ++pass_idx)
            {
                const auto& pass = graph.passes[pass_idx];

                if (!pass.enabled)
                {
//                    LogDebug("[ECS RENDER] Skipping disabled pass %zu (phases %d-%d)",pass_idx, static_cast<int>(pass.startPhase), static_cast<int>(pass.endPhase));
                    continue;
                }

//                LogInfo("[ECS RENDER] Executing pass %zu (phases %d-%d)",pass_idx, static_cast<int>(pass.startPhase), static_cast<int>(pass.endPhase));

                // pass.renderTarget 尚未生效（命令缓冲与提交同步未支持跨 RT 切换）。
                // 此处显式告警，避免"设置了却被静默忽略"导致的难查问题。
                if (pass.renderTarget && pass.renderTarget != GetRenderTarget())
                {
                    LogWarning("[ECS RENDER] Pass %zu requests a different render target, "
                               "but per-pass RT switching is not supported yet; "
                               "pass will render to the frame's current target.",
                               pass_idx);
                }

                if (pass.onBeforePass)
                {
//                    LogDebug("[ECS RENDER] Invoking onBeforePass for pass %zu", pass_idx);
                    pass.onBeforePass(*this, pass);
                }

                if (pass.runUpdate)
                {
                    const ExecutionPhase update_min =
                        std::max(pass.startPhase, ExecutionPhase::RenderDrawSubmit);
                    if (update_min <= pass.endPhase)
                    {
                        //HGL_CAPTURE_SCOPE();
                        //LogDebug("[ECS RENDER] Update phase range %d to %d (clamped from %d)",
                        //        static_cast<int>(update_min), static_cast<int>(pass.endPhase),
                        //        static_cast<int>(pass.startPhase));
                        RunRenderPhaseUpdates(update_min, pass.endPhase, deltaTime);
                    }
                }

                if (pass.runRender)
                {
                    HGL_CAPTURE_SCOPE();
                    RecordPreparedRenderPhaseRange(pass.startPhase,pass.endPhase,deltaTime,pass.submitTransforms,"[ECS RENDER] Render");
                }
                else
                if (pass.submitTransforms)
                {
                    if (auto transform_system = GetSystem<TransformSystem>())
                        transform_system->SubmitTransformUpdates();
                }

                if (pass.onAfterPass)
                {
//                    LogDebug("[ECS RENDER] Invoking onAfterPass for pass %zu", pass_idx);
                    pass.onAfterPass(*this, pass);
                }

//                LogDebug("[ECS RENDER] Completed pass %zu", pass_idx);
            }
        }

        void ECSContext::Render(float deltaTime, const RenderGraph& graph, const std::function<void(float)>& pre_render)
        {
            if (!active)
                return;

            // Canonical frame entry with RenderGraph
//            LogInfo("[ECS RENDER] ===== Frame Start (RenderGraph with %zu passes) =====", graph.GetEnabledPassCount());

            // ── 黄金路径：在主帧 AcquireSwapchainImage 之前执行场景预处理 Pass（主光 CSM 阴影）──
            ExecuteScenePrePassWorkflow(deltaTime);

            if (!BeginManagedRenderFrame(deltaTime))
                return;

            ExecuteRenderGraphPasses(graph, deltaTime, pre_render);
            EndManagedRenderFrame(deltaTime);

//            LogInfo("[ECS RENDER] ===== Frame End (RenderGraph) =====");
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

//            MLogDebug(RenderGraph,"[RenderGraph] Scene stats: detected %zu active render groups",stats.active_render_groups.size());
            for (const auto& group_name : stats.active_render_groups)
            {
                MLogDebug(RenderGraph,"[RenderGraph]   active group: %s", group_name.c_str());
            }

            return stats;
        }

        RenderGraph CreateAdaptiveRenderGraph(ECSContext* context, const SceneStats& stats)
        {
            RenderGraph graph;

//            MLogDebug(RenderGraph,"[RenderGraph] Adaptive: detected %zu active groups",stats.active_render_groups.size());

            auto& registry = SystemGroupRegistry::Get();
            EnsureSystemGroupsRegistered(context);

            // 组启用状态由**本世界的 stats** 即时推导（不再写入全局注册表——
            // enabled 是每世界状态，全局存储会在多世界建图时互相覆盖；
            // registry 只保留组定义+installer 这类真正的全局不变量）。
            const auto all_groups = registry.GetAllGroups();
            for (const auto& group : all_groups)
            {
                const bool enabled = stats.HasGroup(group.name);

                if (context)
                {
                    context->SetElementTypeSystemsEnabled(group.name, enabled);
                }

                if (!enabled)
                    continue;

//                MLogDebug(RenderGraph,"[RenderGraph] Adding pass for group '%s' (phases %d-%d)",group.name.c_str(),static_cast<int>(group.startPhase),static_cast<int>(group.endPhase));

                // Each enabled group becomes a pass in the graph
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

    }//namespace ecs
}//namespace hgl
