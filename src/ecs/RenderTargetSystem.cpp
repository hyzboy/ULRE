#include<hgl/ecs/RenderTargetSystem.h>
#include<hgl/ecs/Context.h>
#include<hgl/ecs/CameraSystem.h>
#include<hgl/ecs/LineRenderSystem.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/VKRenderTarget.h>

namespace hgl::ecs
{
    RenderTargetSystem::RenderTargetSystem(const std::string &name)
        : System(name)
    {
        SetExecutionOrder(ExecutionPhase::RenderPreBeginFrame, ExecutionPriority::First);
    }

    void RenderTargetSystem::SetRenderFramework(graph::RenderFramework *rf)
    {
        render_framework = rf;
    }

    void RenderTargetSystem::SetRenderTarget(graph::IRenderTarget *rt)
    {
        render_target = rt;
        SyncSubsystems();
    }

    void RenderTargetSystem::Update(float /*deltaTime*/)
    {
        if (!render_target)
            return;

        if (!render_framework)
            render_framework = render_target->GetRenderFramework();

        SyncSubsystems();
    }

    void RenderTargetSystem::SyncSubsystems()
    {
        if (!context)
            return;

        auto camera_system = context->GetSystem<CameraSystem>();
        if (camera_system)
        {
            camera_system->SetRenderFramework(render_framework);
            camera_system->SetViewportInfo(render_target ? render_target->GetViewportInfo() : nullptr);
        }

        auto line_system = context->GetSystem<LineRenderSystem>();
        if (line_system)
        {
            line_system->SetRenderFramework(render_framework);
            line_system->SetRenderTarget(render_target);
        }
    }
}//namespace hgl::ecs
