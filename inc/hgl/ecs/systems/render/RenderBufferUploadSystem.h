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
        * Executes staged buffer uploads (BufferUpdateQueue::FlushAll) and inserts
        * a memory barrier before render pass begins.
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
