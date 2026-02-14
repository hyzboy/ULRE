#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/LineRenderSystem.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/vk/VKRenderTarget.h>

namespace hgl::ecs
{
    RenderTargetSystem::RenderTargetSystem(const std::string &name)
        : System(name)
    {
        SetExecutionOrder(ExecutionPhase::RenderPreBeginFrame, ExecutionPriority::First);
    }

    void RenderTargetSystem::SetRenderContext(graph::RenderContext *ctx)
    {
        render_context = ctx;
    }

    void RenderTargetSystem::SetRenderTarget(graph::IRenderTarget *rt)
    {
        render_target = rt;
        SyncSubsystems();
    }

    void RenderTargetSystem::Update(float /*deltaTime*/)
    {
        if (!render_context && context)
            render_context = context->GetRenderContext();

        if (!render_target && context)
            render_target = context->GetRenderTarget();

        if (!render_target)
            return;

        SyncSubsystems();
    }

    void RenderTargetSystem::SyncSubsystems()
    {
        if (!context)
            return;

        if (render_context && render_target)
            render_context->SetCurrentRenderTarget(render_target);

        auto camera_system = context->GetSystem<CameraSystem>();
        if (camera_system)
        {
            camera_system->SetRenderContext(render_context);
            camera_system->SetViewportInfo(render_target ? render_target->GetViewportInfo() : nullptr);
        }

        auto line_system = context->GetSystem<LineRenderSystem>();
        if (line_system)
        {
            line_system->SetRenderContext(render_context);
            line_system->SetRenderTarget(render_target);
        }
    }
}//namespace hgl::ecs

