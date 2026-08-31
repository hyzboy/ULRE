#pragma once

#include <string>
#include <memory>
#include <vector>

namespace hgl::graph
{
    class RenderCmdBuffer;
}

namespace hgl::ecs
{
    class ECSContext;

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

        /// RenderDrawSubmit phase: record GPU draw commands for all batches
        /// @param cmd: render command buffer (inside open render pass)
        virtual void Render(graph::RenderCmdBuffer* cmd) = 0;

        /// Shutdown and cleanup thread-local resources
        virtual void Shutdown() {}

    protected:
        RenderPipelineBase() = default;
    };

}//namespace hgl::ecs
