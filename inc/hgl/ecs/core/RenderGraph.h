#pragma once

#include <hgl/ecs/core/System.h>
#include <hgl/ecs/core/SystemGroup.h>
#include <hgl/type/FNV1a.h>
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
         * Initialize system groups from currently registered systems in context.
         */
        void InitializeSystemGroups(ECSContext* context);

        /// Backward-compatible alias
        inline void InitializeRenderSystemGroups(ECSContext* context) { InitializeSystemGroups(context); }

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
                uint64_t hash = hgl::hash::FNV1aInit<uint64_t>();
                for (const auto& group_name : active_render_groups)
                {
                    const uint64_t value = static_cast<uint64_t>(std::hash<std::string>{}(group_name));
                    hash = hgl::hash::FNV1aAppend(hash, value);
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

    } // namespace ecs
} // namespace hgl
