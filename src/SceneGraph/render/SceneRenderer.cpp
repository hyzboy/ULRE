#include<hgl/graph/SceneRenderer.h>
#include<hgl/graph/Scene.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/camera/Camera.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/geo/line/LineRenderManager.h>

namespace hgl::graph
{
    extern LineRenderManager *CreateLineRenderManager(RenderFramework *,IRenderTarget *); // forward factory

    SceneRenderer::SceneRenderer(RenderFramework *rf,IRenderTarget *rt)
    {
        render_target=rt;
        scene=nullptr;

        frame_camera=new FrameCamera(rf);

        camera_control=nullptr;

        render_task=new RenderTask("DefaultRenderTask",rt);

        if(rf && rt)
            line_render_manager=CreateLineRenderManager(rf,rt);

        clear_color.Set(0,0,0,1);
    }

    SceneRenderer::~SceneRenderer()
    {
        SAFE_CLEAR(render_task);
        SAFE_CLEAR(line_render_manager);
        SAFE_CLEAR(frame_camera);
    }

    bool SceneRenderer::SetRenderTarget(IRenderTarget *rt)
    {
        if(render_target==rt)
            return(true);

        render_target=rt;

        if(frame_camera)
            frame_camera->SetViewportInfo(render_target?render_target->GetViewportInfo():nullptr);

        if(camera_control && render_target)
            camera_control->SetViewport(render_target->GetViewportInfo());

        render_task->SetRenderTarget(rt);

        if(render_target)
        {
            if(!line_render_manager)
                line_render_manager=CreateLineRenderManager(scene->GetRenderFramework(),rt);
            else
                line_render_manager->SetRenderTarget(rt);
        }

        return(true);
    }

    void SceneRenderer::SetScene(Scene *sw)
    {
        if(scene==sw)
            return;

        scene=sw;
    }

    void SceneRenderer::SetCameraControl(CameraControl *cc)
    {
        if(camera_control==cc)
            return;

        if(camera_control)
            camera_control->SetCamera(nullptr,nullptr);

        camera_control=cc;

        if(camera_control && frame_camera)
        {
            if(render_target)
                camera_control->SetViewport(render_target->GetViewportInfo());

            camera_control->SetCamera(frame_camera->GetCamera(),frame_camera->GetCameraInfo());
        }

        render_task->SetCameraInfo(GetCameraInfo());
    }

    void SceneRenderer::Tick(double delta)
    {
        if(camera_control && frame_camera)
        {
            camera_control->Refresh();
            frame_camera->UpdateUBO();
        }

        if(scene)
            scene->Tick(delta);
    }
    
    bool SceneRenderer::RenderFrame()
    {
        if(!scene)
            return(false);

        SceneNode *root = scene->GetRootNode();

        if(!root)
            return(false);

        root->UpdateWorldTransform();

        render_task->RebuildRenderList(root);

        bool result = false;

        RenderCmdBuffer *cmd = render_target->BeginRender();

        if(frame_camera)
            cmd->SetDescriptorBinding(frame_camera->GetDescriptorBinding());
        scene->BindDescriptor(cmd);

        cmd->SetClearColor(0,clear_color);

        cmd->BeginRenderPass();

        result=render_task->Render(cmd);

        if(line_render_manager)
            line_render_manager->Draw(cmd);

        cmd->EndRenderPass();

        render_target->EndRender();

        render_state_dirty = result;

        return(result);
    }

    bool SceneRenderer::Submit()
    {
        if(!render_target)
            return(false);

        if(!render_state_dirty)
            return(false);

        return render_target->Submit();
    }
}//namespace hgl::graph
