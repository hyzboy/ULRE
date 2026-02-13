#include<hgl/graph/RenderContext.h>
#include<hgl/graph/World.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/ecs/CameraSystem.h>

namespace hgl::graph
{
    extern LineRenderManager *CreateLineRenderManager(RenderFramework *,IRenderTarget *); // forward factory

    RenderContext::RenderContext(RenderFramework *rf,IRenderTarget *rt)
    {
        this->rf = rf;
        render_target = rt;
        ecs_context = nullptr;
        viewport_info = rt ? rt->GetViewportInfo() : nullptr;

        if(rf && rt)
            line_render_mgr = CreateLineRenderManager(rf,rt);
    }

    RenderContext::~RenderContext()
    {
        SAFE_CLEAR(line_render_mgr);
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

    void RenderContext::SetRenderTarget(IRenderTarget *rt)
    {
        render_target = rt;
        viewport_info = rt ? rt->GetViewportInfo() : nullptr;

        SyncCameraSystem(ecs_context, rf, viewport_info);

        if(render_target)
        {
            if(!line_render_mgr)
                line_render_mgr = CreateLineRenderManager(rf,rt);
            else
                line_render_mgr->SetRenderTarget(rt);
        }
    }

    void RenderContext::SetECSContext(ecs::ECSContext *ctx)
    {
        ecs_context = ctx;
        SyncCameraSystem(ecs_context, rf, viewport_info);
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
