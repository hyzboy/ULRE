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
     * All pipelines follow the same multi-phase pattern:
     *   1. PrepareFrame() — initialize per-frame state (optional phase before Collect)
     *   2. RunCollect() — RenderCollect phase: gather visible components
     *   3. RunCull() — RenderCull phase: frustum/occlusion culling (optional)
     *   4. RunSort() — RenderSort phase: depth/distance sorting (optional)
     *   5. RunBuild() — RenderBatch phase: write VABs/batches, mark buffers dirty
     *   6. RunSync() — RenderBufferUpload/FrameSync phase: sync descriptors/UBOs
     *   7. GetRenderPrimitives() — query scheduled primitives for draw call recording
     *   8. Render(cmd) — RenderDrawSubmit phase: record GPU draw commands
     * 
     * Systems call these methods in order, no internal system ordering needed.
     * GraphicsPipeline maintains all per-frame state (collected items, batches, etc.)
     * and makes it available to systems at different phases.
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

        /// RenderCull phase: frustum/occlusion/etc. culling (optional for some pipelines)
        virtual void RunCull() {}

        /// RenderSort phase: depth/distance sorting (optional for some pipelines)
        virtual void RunSort() {}

        /// RenderBatch phase: build batches, write VABs, mark buffers dirty
        virtual void RunBuild() = 0;

        /// RenderBufferUpload/FrameSync phase: sync descriptors, UBOs after upload (optional)
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
