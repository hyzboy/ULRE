#include<hgl/ecs/core/render/RenderGraph.h>
#include<hgl/ecs/core/SystemGroup.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/systems/render/frame/RenderSystemCore.h>
#include<hgl/ecs/support/storage/FrameProfiler.h>
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

            // 槽口径：只包「pass 循环」——录制绘制命令（RunRenderPhaseUpdates 的
            // RenderDrawSubmit.. 段 + RecordPreparedRenderPhaseRange）。pre_render 回调
            // 属应用侧，已在上方先执行，不计入本槽。
            FrameProfileSpan draw_span(FrameProfileSlot::DrawRecord);

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

            auto& registry = SystemGroupRegistry::Get();
            EnsureSystemGroupsRegistered(context);

            // 组启用状态由**本世界的 stats** 即时推导（enabled 是每世界状态，不写全局注册表）。
            // 它只控制「无活动组件时跳过该系统」的空跑开销；**不再**决定 pass 数量/顺序。
            for (uint32_t group_idx = 1; group_idx < kSystemGroupCount; ++group_idx)
            {
                const SystemGroupID group_id = static_cast<SystemGroupID>(group_idx);
                const SystemGroup* group = registry.GetGroup(kSystemGroupNames[group_idx]);

                if (context)
                {
                    const bool enabled = group && stats.HasGroup(group->name);
                    context->SetSystemGroupEnabled(group_id, enabled);
                }
            }

            // 注册表里存在但未列入 kSystemGroupNames 的组（理论上不该发生）：
            // 系统侧 SetRenderElementType 与组名表必须同名，否则此处会给出告警。
            for (const auto& group : registry.GetAllGroups())
            {
                if (SystemGroupFromName(group.name.c_str()) == SystemGroupID::None)
                {
                    MLogWarning(RenderGraph,
                        "[RenderGraph] 未知系统组名 '%s'（未在 kSystemGroupNames 里）⇒ 本组不启用；"
                        "系统侧 SetRenderElementType 与组名表必须同名",
                        group.name.c_str());
                }
            }

            // ── 固定分段表（替代历史「按活动组派生 pass」）─────────────────────
            //
            // 渲染顺序由**固定、互不重叠**的相位分段定义，与场景内容无关：
            //   RenderDrawSubmit  → Scene（深度测试的 3D 场景图元/线条/常规文本）
            //   RenderDebug       → Gizmo（无深度测试、必须压在最上层的覆盖图元）
            //   RenderUI2D        → 2D UI（sprite 桥接的 2D 图元 / 常规 HUD 文本）
            //   RenderOverlay     → 最终全屏覆盖层（DEBUG PROF 统计文本）
            //   RenderStat        → 统计系统
            //
            // 每个 pass 只覆盖**单一**相位，相位互不重叠 ⇒ 每个系统每帧只执行一次。
            // 这同时修复了两个缺陷：
            //   1) gizmo 回归：旧逻辑最后一个 pass 的 endPhase < RenderDebug，
            //      使 RenderDebug 相位的 PrimitiveOverlayRenderSystem 不再重绘，
            //      gizmo 被后续普通图元盖掉。
            //   2) N× 重录：旧逻辑「每活动组一个 pass、pass 不做组过滤」，
            //      导致整段绘制区间按活动组数量重复录制。
            static const ExecutionPhase kFixedSegments[] =
            {
                ExecutionPhase::RenderDrawSubmit,
                ExecutionPhase::RenderDebug,
                ExecutionPhase::RenderUI2D,
                ExecutionPhase::RenderOverlay,
                ExecutionPhase::RenderStat,
            };

            for (const ExecutionPhase phase : kFixedSegments)
            {
                graph.Add(RenderGraph::Pass(phase, phase));
            }

            return graph;
        }

    }//namespace ecs
}//namespace hgl
