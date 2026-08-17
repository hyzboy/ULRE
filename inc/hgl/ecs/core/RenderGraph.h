#pragma once

#include <hgl/ecs/core/System.h>
#include <hgl/ecs/core/SystemGroup.h>
#include <vector>
#include <functional>
#include <set>

namespace hgl
{
    namespace graph
    {
        class IRenderTarget;
    }

    namespace ecs
    {
        class ECSContext;

        /**
         * RenderGraph defines a sequence of render passes to be executed
         * Each pass can target a different render target and has custom setup
         * This enables multi-RT rendering, conditional passes, and flexible render topology
         */
        struct RenderGraph
        {
            /**
             * A single render pass within the graph
             */
            struct Pass
            {
                /// Starting ExecutionPhase for this pass
                ExecutionPhase startPhase = static_cast<ExecutionPhase>(0);

                /// Ending ExecutionPhase for this pass (inclusive)
                ExecutionPhase endPhase = static_cast<ExecutionPhase>(0);

                /// Target render target (nullptr = current/swapchain)
                hgl::graph::IRenderTarget* renderTarget = nullptr;

                /// Whether this pass should execute
                bool enabled = true;

                /// Whether this pass should execute Update() over the phase range
                bool runUpdate = true;

                /// Whether this pass should submit transform data before Render()
                bool submitTransforms = true;

                /// Whether this pass should execute Render() over the phase range
                bool runRender = true;

                /// Optional setup callback invoked before running this pass
                /// Useful for clearing colors, setting stencil state, etc.
                /// Signature: void(ECSContext& context, const Pass& pass)
                std::function<void(ECSContext&, const Pass&)> onBeforePass = nullptr;

                /// Optional cleanup callback invoked after running this pass
                std::function<void(ECSContext&, const Pass&)> onAfterPass = nullptr;

                Pass() = default;

                Pass(ExecutionPhase start, ExecutionPhase end,
                     hgl::graph::IRenderTarget* rt = nullptr,
                                         bool en = true,
                                         bool update = true,
                                         bool submit = true,
                                         bool render = true)
                                        : startPhase(start),
                                            endPhase(end),
                                            renderTarget(rt),
                                            enabled(en),
                                            runUpdate(update),
                                            submitTransforms(submit),
                                            runRender(render) {}
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

            /// Clear all passes
            void Clear()
            {
                passes.clear();
            }

            /// Get totalpass count (including disabled ones)
            size_t GetPassCount() const { return passes.size(); }

            /// Get count of enabled passes
            size_t GetEnabledPassCount() const;
        };

        /**
         * Ensure system groups are registered for the currently registered systems in context.
         * Idempotent: registers only groups that are missing (no Clear, no full rescan).
         * Group records feed pass construction in CreateAdaptiveRenderGraph/
         * CreateDefaultLinearGraph; the installer path (DefaultSystems) registers
         * systems/pipelines, this fills in the group metadata once.
         */
        void EnsureSystemGroupsRegistered(ECSContext* context);

        /**
         * Create the default linear render graph with all system groups enabled
         * Maintains backward compatibility with existing render flow
         */
        RenderGraph CreateDefaultLinearGraph(ECSContext* context);

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

            // Returns a hash for caching decisions
            uint64_t GetHash() const
            {
                uint64_t hash = 1469598103934665603ULL;
                for (const auto& group_name : active_render_groups)
                {
                    const uint64_t value = static_cast<uint64_t>(std::hash<std::string>{}(group_name));
                    hash ^= value;
                    hash *= 1099511628211ULL;
                }
                return hash;
            }
        };

        /**
         * Gather scene component statistics. Call this to understand what's in the world.
         */
        SceneStats GatherSceneStats(ECSContext* context);

        /**
         * Create RenderGraph adapted to scene content.
         * Skips render phases and systems for content types not present.
         */
        RenderGraph CreateAdaptiveRenderGraph(ECSContext* context);

        /**
         * Create RenderGraph adapted to scene content using pre-gathered stats.
         * Callers that already hold a SceneStats (e.g. the per-frame adaptive path)
         * pass it here to avoid a second full entity scan.
         */
        RenderGraph CreateAdaptiveRenderGraph(ECSContext* context, const SceneStats& stats);

    } // namespace ecs
} // namespace hgl
