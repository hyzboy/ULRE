#pragma once

#include <string>
#include <memory>
#include <vector>

namespace hgl::graph
{
    class RenderCmdBuffer;
    class Primitive;
}

namespace hgl::ecs
{
    class ECSContext;

    /**
     * RenderPipelineBase - Abstract base class for all render pipelines
     *
     * Provides a unified interface for geometry/text/line/billboard/particle rendering.
     * The current ECS execution flow is:
     *   1. PrepareFrame() — initialize per-frame state before collection (optional)
     *   2. RunCollect() — RenderCollect: gather visible components
     *   3. RunCull() — optional culling step, typically called by a collect-stage system
     *   4. RunSort() — optional sort step, typically called in RenderBatch stage
     *   5. RunBuild() — RenderBatch: write VABs/batches, mark buffers dirty
     *   6. RunSync() — RenderFrameSync: sync descriptors/UBOs after buffer upload
     *   7. GetRenderPrimitives() — query scheduled primitives for draw call recording
     *   8. Render(cmd) — RenderDrawSubmit: record GPU draw commands
     *
     * Systems call these methods in order while ECS controls phase scheduling.
     * RenderPipelineBase stores per-frame state (collected items, batches, etc.)
     * that is shared across systems in different phases.
     */
    class RenderPipelineBase
    {
    public:
        virtual ~RenderPipelineBase() = default;

        /// Get pipeline name (e.g., "Primitive", "Text", "Line", "Quad")
        virtual const std::string& GetName() const = 0;

        /// Get associated ECS context (if any)
        virtual ECSContext* GetWorld() const = 0;

        /// Initialize per-frame state before Collect phase
        /// Called once per frame before any render systems execute
        /// @return false if frame setup failed (e.g., no visible content)
        virtual bool PrepareFrame() = 0;

        /// RenderCollect phase: gather visible components
        virtual void RunCollect() = 0;

        /// Optional culling step, usually triggered by a collect-stage system
        virtual void RunCull() {}

        /// Optional sorting step, usually triggered by a batch-stage system
        virtual void RunSort() {}

        /// RenderBatch phase: build batches, write VABs, mark buffers dirty
        virtual void RunBuild() = 0;

        /// RenderFrameSync phase: sync descriptors/UBOs after buffer upload (optional)
        virtual void RunSync() {}

        /// Get all primitives scheduled for this frame (for recording draw calls)
        /// @param out_primitives: filled with pointers to Primitive objects
        virtual void GetRenderPrimitives(std::vector<graph::Primitive*>& out_primitives) const = 0;

        /// RenderDrawSubmit phase: record GPU draw commands for all batches
        /// @param cmd: render command buffer (inside open render pass)
        virtual void Render(graph::RenderCmdBuffer* cmd) = 0;

        /// Shutdown and cleanup thread-local resources
        virtual void Shutdown() {}

    protected:
        RenderPipelineBase() = default;
    };

}//namespace hgl::ecs
