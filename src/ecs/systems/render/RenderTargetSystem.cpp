#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/LineRenderSystem.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/vk/VKRenderTarget.h>

namespace hgl::ecs
{
    RenderTargetSystem::RenderTargetSystem(const std::string &name)
        : System(name)
    {
        SetExecutionOrder(ExecutionPhase::RenderPreBeginFrame, ExecutionPriority::First);
    }

    void RenderTargetSystem::SetGraphicsContext(graph::IGraphicsContext *gc)
    {
        graphics_context = gc;
    }

    void RenderTargetSystem::SetRenderTarget(graph::IRenderTarget *rt)
    {
        render_target = rt;
        SyncSubsystems();
    }

    void RenderTargetSystem::Update(float /*deltaTime*/)
    {
        if (!graphics_context && context)
            graphics_context = context->GetGraphicsContext();

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

        auto camera_system = context->GetSystem<CameraSystem>();
        if (camera_system)
        {
            camera_system->SetGraphicsContext(graphics_context);
            camera_system->SetViewportInfo(render_target ? render_target->GetViewportInfo() : nullptr);
        }

        auto line_system = context->GetSystem<LineRenderSystem>();
        if (line_system)
        {
            line_system->SetGraphicsContext(graphics_context);
            line_system->SetRenderTarget(render_target);
        }
    }
}//namespace hgl::ecs

