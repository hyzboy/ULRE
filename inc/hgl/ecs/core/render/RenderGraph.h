#pragma once

#include <hgl/ecs/core/System.h>
#include <hgl/ecs/core/SystemGroup.h>
#include <hgl/util/hash/FNV1a.h>
#include <hgl/type/String.h>
#include <vector>
#include <functional>
#include <set>

namespace hgl
{
    namespace ecs
    {
        class ECSContext;

        /**
         * RenderGraph defines a sequence of render passes to be executed
         * 每个 pass 覆盖一段 ExecutionPhase 区间并带自己的 setup 回调。
         *
         * ⚠ **一个 pass ≠ 一个新的渲染目标**（A3 结案 2026-10-07）：引擎的多 RT 机制是
         * `ECSContext::RenderTo(rt, ...)` + RenderPassOptions（CSM 与离屏世界都走它，
         * 且已含槽带/帧槽分份），不在 pass 上。历史字段 `Pass::renderTarget` 从未被任何
         * 调用方设置过、执行器只打印一条永不可能触发的告警 ⇒ 已整体删除。
         * 将来做后处理链，请基于 `RenderTo`，不要复活该字段。
         */
        struct RenderGraph
        {
            /**
             * A single render pass within the graph
             */
            struct Pass
            {
                /// Starting ExecutionPhase for this pass
                ///
                /// ⚠ **语义定案（2026-10-11，§3-③）**：`ECSContext::ExecuteRenderGraphPasses`
                /// 只从 `max(startPhase, ExecutionPhase::RenderDrawSubmit)` 起跑本 pass 的相位更新。
                /// 这**不是**"声明在 `RenderCollect` 起点的更新被忽略/漏执行"，而是**避免重跑**：
                /// 帧首的 `BeginManagedRenderFrame`（`ContextRender.cpp:838-862`）已按序跑过
                /// `SubmitTransformUpdates → PublishCameraRows → RenderCollect → SyncWorldAddresses
                ///  → RenderBatch → RenderBufferCommit → RenderBufferUpload → RenderFrameSync`。
                /// 契约 = 「pass 相位区间中 **< `RenderDrawSubmit`** 的部分由帧首前置段执行」。
                ExecutionPhase startPhase = static_cast<ExecutionPhase>(0);

                /// Ending ExecutionPhase for this pass (inclusive)
                ExecutionPhase endPhase = static_cast<ExecutionPhase>(0);

                Pass() = default;

                /// 只剩 (起点相位, 终点相位)。
                /// 原先的 `enabled` / `runUpdate` / `submitTransforms` / `runRender` /
                /// `onBeforePass` / `onAfterPass` 六个字段**从无赋值点**（唯一构造点传全 true/空，
                /// 全仓无任何 `pass.X = ...`）⇒ 2026-10-10 随 §1.1 删除；执行路径同时改回无条件调用
                /// （与恒 true/空的旧行为逐字等价）。
                Pass(ExecutionPhase start, ExecutionPhase end)
                    : startPhase(start),
                      endPhase(end) {}
            };

            /// Ordered sequence of render passes
            std::vector<Pass> passes;

            RenderGraph() = default;
            ~RenderGraph() = default;

            /// Add a pass to the graph
            void Add(const Pass& pass)
            {
                passes.push_back(pass);
            }
        };

        /**
         * Ensure system groups are registered for the currently registered systems in context.
         * Idempotent: registers only groups that are missing (no Clear, no full rescan).
         * Group records feed pass construction in CreateAdaptiveRenderGraph;
         * the installer path (DefaultSystems) registers
         * systems/pipelines, this fills in the group metadata once.
         */
        void EnsureSystemGroupsRegistered(ECSContext* context);

        /**
         * Scene statistics for adaptive graph generation
         */
        struct SceneStats
        {
            std::set<std::string> active_render_groups;

            bool HasGroup(const std::string& group_name) const
            {
                return active_render_groups.find(group_name) != active_render_groups.end();
            }

            // Returns a hash for caching decisions using HGL FNV1a
            uint64_t GetHash() const
            {
                hgl::hash::FNV1aHasher64 h;
                for (const auto& group_name : active_render_groups)
                {
                    h.AppendBytes(group_name.data(), group_name.size());
                }
                return h;
            }
        };

        /**
         * Gather scene component statistics. Call this to understand what's in the world.
         */
        SceneStats GatherSceneStats(ECSContext* context);

        /**
         * Create RenderGraph adapted to scene content using pre-gathered stats.
         * Callers that already hold a SceneStats (e.g. the per-frame adaptive path)
         * pass it here to avoid a second full entity scan.
         */
        RenderGraph CreateAdaptiveRenderGraph(ECSContext* context, const SceneStats& stats);

    } // namespace ecs
} // namespace hgl
