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
        extent_valid = false;
        SyncSubsystems();
    }

    void RenderTargetSystem::Update(float /*deltaTime*/)
    {
        if (!render_target)
            return;

        if (!render_framework)
            render_framework = render_target->GetRenderFramework();

        SyncViewport();
        SyncSubsystems();

        if (context)
        {
            auto camera_system = context->GetSystem<CameraSystem>();
            if (camera_system)
                camera_system->SyncCameraUBO();
        }
    }

    void RenderTargetSystem::SyncViewport()
    {
        if (!render_target)
            return;

        const VkExtent2D &ext = render_target->GetExtent();
        if (!extent_valid || ext.width != last_width || ext.height != last_height)
        {
            render_target->OnResize(ext);
            last_width = ext.width;
            last_height = ext.height;
            extent_valid = true;
        }
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
