#include<hgl/ecs/core/RenderGraph.h>
#include<hgl/ecs/core/SystemGroup.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/systems/render/RenderSystemCore.h>
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

                // A3 结案（2026-10-07）：pass 级跨 RT 切换已删。多 RT 走 ECSContext::RenderTo()。
                // 2026-10-10（§1.1）：六个 pass 配置字段无赋值点 ⇒ 删除，此处恢复**无条件执行**
                // （与恒 true / 恒空逐字等价；原 `else if (submitTransforms)` 分支因 runRender 恒 true
                //  而永不可达，一并删除）。

                {
                    // §3-③（2026-10-11 定案）：抬高下界 = **避免重跑**，不是漏执行。
                    // 帧首 `BeginManagedRenderFrame` 已跑完 Collect/Batch/Commit/Upload/FrameSync
                    // （`ContextRender.cpp:838-862`）⇒ 本 pass 只从 `RenderDrawSubmit` 起补跑。
                    // 详见 `RenderGraph.h` 的 `Pass::startPhase` 头注（契约口径在那里）。
                    const ExecutionPhase update_min =
                        std::max(pass.startPhase, ExecutionPhase::RenderDrawSubmit);
                    if (update_min <= pass.endPhase)
                    {
                        RunRenderPhaseUpdates(update_min, pass.endPhase, deltaTime);
                    }
                }

                RecordPreparedRenderPhaseRange(pass.startPhase,pass.endPhase,deltaTime,true,"[ECS RENDER] Render");

//                LogDebug("[ECS RENDER] Completed pass %zu", pass_idx);
            }
        }

        void ECSContext::Render(float deltaTime, const RenderGraph& graph, const std::function<void(float)>& pre_render)
        {
            if (!active)
                return;

            // Canonical frame entry with RenderGraph
//            LogInfo("[ECS RENDER] ===== Frame Start (RenderGraph with %zu passes) =====", graph.GetEnabledPassCount());

            // ── 场景预处理（主光 CSM 阴影）已内聚到 BeginManagedRenderFrame（D6）──
            // 主帧入口统一执行，杜绝"新入口忘记调用 ⇒ 阴影静默失效"。见 Context.cpp。
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

            bool active_groups[kSystemGroupCount] = {};

            // 零堆分配结构性遍历：消除逐帧 vector<const Entity*> 堆分配与 set::emplace 频繁查找
            context->ForEachAliveEntity([&active_groups](const Entity* entity)
            {
                if (!entity)
                    return;

                for (uint32_t type = 1; type < COMPONENT_TYPE_COUNT; ++type)
                {
                    if (!entity->GetComponentRow(static_cast<ComponentType>(type)))
                        continue;

                    const SystemGroupID group = GetComponentGroup(static_cast<ComponentType>(type));
                    const uint32_t group_idx = static_cast<uint32_t>(group);
                    if (group != SystemGroupID::None && group_idx < kSystemGroupCount)
                    {
                        active_groups[group_idx] = true;
                    }
                }
            });

            for (uint32_t i = 1; i < kSystemGroupCount; ++i)
            {
                if (active_groups[i])
                {
                    stats.active_render_groups.emplace(kSystemGroupNames[i]);
                }
            }

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
                    const SystemGroupID group_id = SystemGroupFromName(group.name.c_str());

                    if (group_id != SystemGroupID::None)
                    {
                        context->SetSystemGroupEnabled(group_id, enabled);
                    }
                    else
                    {
                        MLogWarning(RenderGraph,
                            "[RenderGraph] 未知系统组名 '%s'（未在 kSystemGroupNames 里）⇒ 本组不启用；"
                            "系统侧 SetRenderElementType 与组名表必须同名",
                            group.name.c_str());
                    }
                }

                if (!enabled)
                    continue;

//                MLogDebug(RenderGraph,"[RenderGraph] Adding pass for group '%s' (phases %d-%d)",group.name.c_str(),static_cast<int>(group.startPhase),static_cast<int>(group.endPhase));

                // Each enabled group becomes a pass in the graph
                graph.Add(RenderGraph::Pass(
                    group.startPhase,
                    group.endPhase
                ));
            }

            // Fully data-driven:
            // - 组件类型声明它驱动哪个组（`kComponentTypeTable[].group`；T2-c 起组归属是**类型属性**，
            //   组件对象上不再有虚表）
            // - Systems declare their group via System::SetRenderElementType()
            // - RenderGraph builds passes from the name mapping at runtime

            return graph;
        }

    }//namespace ecs
}//namespace hgl
