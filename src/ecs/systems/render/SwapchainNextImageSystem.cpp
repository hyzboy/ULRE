#include<hgl/ecs/systems/render/SwapchainNextImageSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    SwapchainNextImageSystem::SwapchainNextImageSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderSwapchainNextImage, ExecutionPriority::First);
    }

    void SwapchainNextImageSystem::Update(float /*deltaTime*/)
    {
        last_acquire_ok = true;

        if (!context)
            return;

        auto *render_target = context->GetRenderTarget();
        if (!render_target)
            return;

        auto *swapchain_rt = dynamic_cast<graph::SwapchainRenderTarget*>(render_target);
        if (!swapchain_rt)
            return;

        last_acquire_ok = swapchain_rt->NextFrame();
            if (!last_acquire_ok)
            {
                LogWarning("SwapchainNextImageSystem: Swapchain NextFrame failed");
        }
    }
}//namespace hgl::ecs
