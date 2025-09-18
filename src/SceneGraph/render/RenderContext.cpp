#include<hgl/graph/RenderContext.h>
#include<hgl/graph/Scene.h>

namespace hgl::graph
{
    extern LineRenderManager *CreateLineRenderManager(RenderFramework *,IRenderTarget *); // forward factory

    // ---------------- RenderContext ----------------
    RenderContext::RenderContext(RenderFramework *rf,IRenderTarget *rt)
    {
        this->rf = rf;
        render_target = rt;
        frame_camera = new FrameCamera(rf);

        if(rf && rt)
            line_render_mgr = CreateLineRenderManager(rf,rt);

        if(frame_camera && rt)
            frame_camera->SetViewportInfo(rt->GetViewportInfo());
    }

    RenderContext::~RenderContext()
    {
        SAFE_CLEAR(line_render_mgr);
        SAFE_CLEAR(frame_camera);
        camera_control = nullptr; // not owner
        scene = nullptr;
        render_target = nullptr;
    }

    void RenderContext::SetRenderTarget(IRenderTarget *rt)
    {
        if(render_target == rt)
            return;

        render_target = rt;

        if(frame_camera)
            frame_camera->SetViewportInfo(render_target ? render_target->GetViewportInfo() : nullptr);

        if(camera_control && render_target)
            camera_control->SetViewport(render_target->GetViewportInfo());

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

        if(camera_control && frame_camera)
        {
            if(render_target)
                camera_control->SetViewport(render_target->GetViewportInfo());

            camera_control->SetCamera(frame_camera->GetCamera(),frame_camera->GetCameraInfo());
        }
    }

    void RenderContext::Tick(double)
    {
        if(camera_control && frame_camera)
        {
            camera_control->Refresh();
            frame_camera->UpdateUBO();
        }
    }

    void RenderContext::BindDescriptor(RenderCmdBuffer *cmd)
    {
        if(frame_camera)
            cmd->SetDescriptorBinding(frame_camera->GetDescriptorBinding());

        if(scene)
            cmd->SetDescriptorBinding(scene->GetDescriptorBinding());
    }
}//namespace hgl::graph
