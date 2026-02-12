#pragma once

#include<hgl/ecs/System.h>

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
     * RenderBufferCommitSystem
     *
     * Flushes BufferCommitQueue before render submission.
     */
    class RenderBufferCommitSystem : public System
    {
    private:

        ECSContext* world = nullptr;
        graph::VulkanDevice* device = nullptr;

    public:

        RenderBufferCommitSystem(const std::string& name = "RenderBufferCommitSystem");
        ~RenderBufferCommitSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }
        void SetDevice(graph::VulkanDevice* dev) { device = dev; }

        void Update(float deltaTime) override;
    };
}//namespace hgl::ecs
