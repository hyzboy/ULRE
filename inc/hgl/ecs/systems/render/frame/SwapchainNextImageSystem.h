#pragma once

#include<hgl/ecs/systems/render/RenderSystem.h>

namespace hgl
{
    namespace graph
    {
        class IRenderTarget;
    }
}

namespace hgl::ecs
{
    /**
     * SwapchainNextImageSystem
     *
     * Acquires the next swapchain image before BeginFrame.
     */
    class SwapchainNextImageSystem : public RenderSystem
    {
    private:

        bool last_acquire_ok = true;

    public:

        SwapchainNextImageSystem(const std::string& name = "SwapchainNextImageSystem");
        ~SwapchainNextImageSystem() override = default;

    public:

        void Update(float deltaTime) override;

        bool WasSuccessful() const { return last_acquire_ok; }
    };
}//namespace hgl::ecs
