#include<hgl/ecs/systems/render/SwapchainSubmitSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/primitive/PrimitiveRenderSystem.h>
#include<hgl/ecs/support/text/TextRenderSystem.h>
#include<hgl/ecs/support/line/LineRenderSystem.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    SwapchainSubmitSystem::SwapchainSubmitSystem(const std::string& name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderSubmit);
    }

    void SwapchainSubmitSystem::Update(float /*deltaTime*/)
    {
        last_submit_ok = true;

        if (!context)
            return;

        auto *render_target = context->GetRenderTarget();
        if (!render_target)
            return;

        // A1：等待列表由 RenderTo 按当前 RT 语义准备
        //     （离屏 pass 等主帧车道；主帧等本帧各离屏 RT 的车道值）
        uint32_t wait_count = 0;
        const graph::SemaphoreSubmit *waits = context->TakeSubmitWaits(wait_count, render_target->IsSwapchain());

            last_submit_ok = render_target->Submit(waits, wait_count);
        if (!last_submit_ok)
        {
                LogWarning("SwapchainSubmitSystem: RenderTarget submit failed");
        }
    }
}//namespace hgl::ecs
