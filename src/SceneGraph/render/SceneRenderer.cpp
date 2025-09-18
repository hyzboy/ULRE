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

        camera = new Camera();                                 // 新建摄像机
        ubo_camera_info = rf->CreateUBO<UBOCameraInfo>(&mtl::SBS_CameraInfo);
        camera_desc_binding = new DescriptorBinding(DescriptorSetType::Camera);
        camera_desc_binding->AddUBO(ubo_camera_info);

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
        SAFE_CLEAR(camera_desc_binding);
        SAFE_CLEAR(ubo_camera_info);
        delete camera; camera=nullptr;
    }

    bool SceneRenderer::SetRenderTarget(IRenderTarget *rt)
    {
        if(render_target==rt)
            return(true);

        render_target=rt;

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

        // 不再把camera_control传回Scene，Scene已经移除摄像机逻辑
    }

    void SceneRenderer::SetCameraControl(CameraControl *cc)
    {
        if(camera_control==cc)
            return;

        if(camera_control)
            camera_control->SetCamera(nullptr,nullptr);

        camera_control=cc;

        if(camera_control)
        {
            if(render_target)
                camera_control->SetViewport(render_target->GetViewportInfo());
            camera_control->SetCamera(camera,ubo_camera_info->data());
        }

        render_task->SetCameraInfo(GetCameraInfo());
    }

    void SceneRenderer::Tick(double delta)
    {
        // 摄像机更新
        if(camera_control)
        {
            camera_control->Refresh();
            ubo_camera_info->Update();
        }

        if(scene)
            scene->Tick(delta);   // Scene 现在不再更新摄像机，仅做自身逻辑
    }
    
    bool SceneRenderer::RenderFrame()
    {
        if(!scene)
            return(false);

        SceneNode *root = scene->GetRootNode();

        if(!root)
            return(false);

        root->UpdateWorldTransform();

        // 这里内部会将Scene tree展开成RenderCollector,而RenderCollector排序是需要CameraInfo的
        render_task->RebuildRenderList(root);

        bool result = false;

        RenderCmdBuffer *cmd = render_target->BeginRender();

        // 先绑定摄像机，再绑定场景
        if(camera_desc_binding)
            cmd->SetDescriptorBinding(camera_desc_binding);
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
