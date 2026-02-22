#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl
{
    namespace graph
    {
        class VulkanDevice;
    }
}

namespace hgl::ecs
{
    class ECSContext;

    /**
     * RenderBufferUploadSystem
     *
     * Iterates the VulkanDevice's IGPUBuffer registry each frame,
     * calls CopyToDevice on dirty buffers, and inserts a memory barrier
     * before the render pass begins.
     *
     * Replaces the old BufferUpdateQueue + BufferCommitQueue two-queue system.
     */
    class RenderBufferUploadSystem : public System
    {
    private:

        ECSContext* world = nullptr;
        graph::VulkanDevice* device = nullptr;

    public:

        RenderBufferUploadSystem(const std::string& name = "RenderBufferUploadSystem");
        ~RenderBufferUploadSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }
        void SetDevice(graph::VulkanDevice* dev) { device = dev; }
        graph::VulkanDevice* GetDevice() const { return device; }

        void Update(float deltaTime) override;
    };
}//namespace hgl::ecs
