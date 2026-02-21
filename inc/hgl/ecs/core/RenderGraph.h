#pragma once

#include <hgl/ecs/core/System.h>
#include <vector>
#include <functional>

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
         * Create the default linear render graph (the current sequential pipeline)
         * This maintains backward compatibility with existing render flow
         */
        RenderGraph CreateDefaultLinearGraph();

    } // namespace ecs
} // namespace hgl
