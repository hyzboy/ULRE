#include<hgl/graph/RenderContext.h>
#include<hgl/graph/World.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/ecs/CameraSystem.h>
#include<hgl/ecs/LineRenderSystem.h>

namespace hgl::graph
{
    RenderContext::RenderContext(RenderFramework *rf,IRenderTarget *rt)
    {
        this->rf = rf;
        render_target = rt;
        ecs_context = nullptr;
        viewport_info = rt ? rt->GetViewportInfo() : nullptr;
    }

    RenderContext::~RenderContext()
    {
        ecs_context = nullptr;
        world = nullptr;
        render_target = nullptr;
        viewport_info = nullptr;
    }

    static void SyncCameraSystem(hgl::ecs::ECSContext* ctx,
                                 RenderFramework* rf,
                                 const ViewportInfo* viewport)
    {
        if(!ctx)
            return;

        auto camera_system = ctx->GetSystem<hgl::ecs::CameraSystem>();
        if(!camera_system)
            return;

        camera_system->SetRenderFramework(rf);
        camera_system->SetViewportInfo(viewport);
    }

    static void SyncLineRenderSystem(hgl::ecs::ECSContext* ctx,
                                     RenderFramework* rf,
                                     IRenderTarget* rt)
    {
        if(!ctx)
            return;

        auto line_system = ctx->GetSystem<hgl::ecs::LineRenderSystem>();
        if(!line_system)
            return;

        line_system->SetRenderFramework(rf);
        line_system->SetRenderTarget(rt);
    }

    void RenderContext::SetRenderTarget(IRenderTarget *rt)
    {
        render_target = rt;
        viewport_info = rt ? rt->GetViewportInfo() : nullptr;

        SyncCameraSystem(ecs_context, rf, viewport_info);
        SyncLineRenderSystem(ecs_context, rf, render_target);
    }

    void RenderContext::SetECSContext(ecs::ECSContext *ctx)
    {
        ecs_context = ctx;
        SyncCameraSystem(ecs_context, rf, viewport_info);
        SyncLineRenderSystem(ecs_context, rf, render_target);
    }

    void RenderContext::Tick(double)
    {
        if(ecs_context)
        {
            auto camera_system = ecs_context->GetSystem<hgl::ecs::CameraSystem>();
            if(camera_system)
                camera_system->SyncCameraUBO();
        }
    }

    void RenderContext::BindDescriptor(RenderCmdBuffer *cmd)
    {
        if(ecs_context)
        {
            auto camera_system = ecs_context->GetSystem<hgl::ecs::CameraSystem>();
            if(camera_system)
                camera_system->BindDescriptor(cmd);
        }

        if(world)
            cmd->SetDescriptorBinding(world->GetDescriptorBinding());
    }
}//namespace hgl::graph
