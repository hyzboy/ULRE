#include<hgl/graph/RenderContext.h>
#include<hgl/graph/World.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/mtl/UBOCommon.h>

namespace hgl::graph
{
    extern LineRenderManager *CreateLineRenderManager(RenderFramework *,IRenderTarget *); // forward factory

    RenderContext::RenderContext(RenderFramework *rf,IRenderTarget *rt)
    {
        this->rf = rf;
        render_target = rt;
        ecs_context = nullptr;
        viewport_info = rt ? rt->GetViewportInfo() : nullptr;

        if(rf)
        {
            ubo_camera_info = rf->CreateUBO<UBOCameraInfo>(&mtl::SBS_CameraInfo);
            if(ubo_camera_info)
            {
                camera_desc_binding = new DescriptorBinding(DescriptorSetType::Camera);
                if(camera_desc_binding)
                    camera_desc_binding->AddUBO(ubo_camera_info);
            }
        }

        if(rf && rt)
            line_render_mgr = CreateLineRenderManager(rf,rt);
    }

    RenderContext::~RenderContext()
    {
        SAFE_CLEAR(line_render_mgr);
        SAFE_CLEAR(camera_desc_binding);
        SAFE_CLEAR(ubo_camera_info);

        camera_control = nullptr; // not owner
        ecs_context = nullptr;
        world = nullptr;
        render_target = nullptr;
        viewport_info = nullptr;
    }

    void RenderContext::UpdateCamera()
    {
        if(camera_control)
            camera_control->Refresh();

        if(ubo_camera_info)
            ubo_camera_info->Update();
    }

    void RenderContext::SetRenderTarget(IRenderTarget *rt)
    {
        render_target = rt;
        viewport_info = rt ? rt->GetViewportInfo() : nullptr;

        if(camera_control && viewport_info)
        {
            camera_control->SetViewport(viewport_info);
            UpdateCamera();
        }

        if(render_target)
        {
            if(!line_render_mgr)
                line_render_mgr = CreateLineRenderManager(rf,rt);
            else
                line_render_mgr->SetRenderTarget(rt);
        }
    }

    void RenderContext::SetCameraControl(CameraControl *cc)
    {
        if(camera_control == cc)
            return;

        if(camera_control)
            camera_control->SetCamera(nullptr,nullptr);

        camera_control = cc;

        if(camera_control)
        {
            if(viewport_info)
                camera_control->SetViewport(viewport_info);

            camera_control->SetCamera(&camera, ubo_camera_info ? ubo_camera_info->data() : nullptr);

            UpdateCamera();
        }
    }

    void RenderContext::Tick(double)
    {
        UpdateCamera();
    }

    void RenderContext::BindDescriptor(RenderCmdBuffer *cmd)
    {
        if(camera_desc_binding)
            cmd->SetDescriptorBinding(camera_desc_binding);

        if(world)
            cmd->SetDescriptorBinding(world->GetDescriptorBinding());
    }
}//namespace hgl::graph
