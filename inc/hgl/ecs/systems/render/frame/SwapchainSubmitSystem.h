#pragma once

#include<hgl/ecs/core/System.h>

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
     * SwapchainSubmitSystem
     *
     * Submits the frame to the render target (swapchain present).
     */
    class SwapchainSubmitSystem : public System
    {
    private:

        bool last_submit_ok = true;

    public:

        SwapchainSubmitSystem(const std::string& name = "SwapchainSubmitSystem");
        ~SwapchainSubmitSystem() override = default;

    public:

        void Update(float deltaTime) override;

        bool WasSuccessful() const { return last_submit_ok; }
    };
}//namespace hgl::ecs
