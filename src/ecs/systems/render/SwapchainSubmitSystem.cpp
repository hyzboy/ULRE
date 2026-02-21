#include<hgl/ecs/systems/render/SwapchainSubmitSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    SwapchainSubmitSystem::SwapchainSubmitSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderSubmit);
    }

    void SwapchainSubmitSystem::Update(float /*deltaTime*/)
    {
        last_submit_ok = true;

        if (!context)
            return;

        auto *render_target = context->GetRenderTarget();
        if (!render_target)
            return;

            last_submit_ok = render_target->Submit();
        if (!last_submit_ok)
        {
                LogWarning("SwapchainSubmitSystem: RenderTarget submit failed");
        }
    }
}//namespace hgl::ecs
