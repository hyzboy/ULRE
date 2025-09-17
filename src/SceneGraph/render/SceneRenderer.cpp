#include<hgl/graph/SceneRenderer.h>
#include<hgl/graph/Scene.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/camera/Camera.h>
#include<hgl/graph/geo/line/LineRenderManager.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/mtl/UBOCommon.h>

namespace hgl::graph
{
    LineRenderManager *CreateLineRenderManager(RenderFramework *rf,IRenderTarget *);

    SceneRenderer::SceneRenderer(RenderFramework *rf,IRenderTarget *rt)
    {
        render_target=rt;
        scene=nullptr;
        camera = new Camera();

        ubo_camera_info = rf->CreateUBO<UBOCameraInfo>(&mtl::SBS_CameraInfo);
        camera_desc_binding = new DescriptorBinding(DescriptorSetType::Camera);
        camera_desc_binding->AddUBO(ubo_camera_info);

        camera_control=nullptr;

        render_task=new RenderTask("DefaultRenderTask",rt);

        clear_color.Set(0,0,0,1);

        line_render_manager = CreateLineRenderManager(rf,rt);
    }

    SceneRenderer::~SceneRenderer()
    {
        SAFE_CLEAR(line_render_manager)
        delete render_task;

        SAFE_CLEAR(camera_control);
        delete camera_desc_binding;
        delete ubo_camera_info;
        delete camera;
    }

    bool SceneRenderer::SetRenderTarget(IRenderTarget *rt)
    {
        if(render_target==rt)
            return(true);

        if(render_target)
        {
            if(render_target->GetDevice()!=rt->GetDevice())
            {
                //换Device是不允许的，当然这一般也不可能
                return(false);
            }
        }

        render_target=rt;

        if(camera_control)
            camera_control->SetViewport(render_target->GetViewportInfo());

        render_task->SetRenderTarget(rt);

        return(true);
    }

    void SceneRenderer::SetScene(Scene *sw)
    {
        if(scene==sw)
            return;

        //if(scene)
        //{
        //    scene->Unjoin(this);
        //}

        scene=sw;

        if(camera_control)
            scene->SetCameraControl(camera_control);

        //if(scene)
        //{
        //    scene->Join(this);
        //}
    }

    void SceneRenderer::SetCameraControl(CameraControl *cc)
    {
        //if(camera)
        //{
        //    if(scene)
        //        camera->Unjoin(scene);

        //    camera->Unjoin(this);
        //}

        camera_control=cc;

        if(camera_control&&render_target)
            camera_control->SetViewport(render_target->GetViewportInfo());

        if(scene)
            scene->SetCameraControl(camera_control);

        if(camera_control)
            camera_control->SetCamera(camera,ubo_camera_info->data());

        //if(camera)
        //{
        //    if(scene)
        //        camera->Unjoin(scene);

        //    camera->Join(this);
        //}

        render_task->SetCameraInfo(ubo_camera_info->data());
    }

    void SceneRenderer::Tick(double delta)
    {
        if(camera_control)
        {
            camera_control->Refresh();

            if(ubo_camera_info)
                ubo_camera_info->Update();
        }
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

        cmd->SetDescriptorBinding(camera_desc_binding);

        if(scene)
        {
            cmd->SetDescriptorBinding(scene->GetDescriptorBinding());
        }

        cmd->SetClearColor(0,clear_color);

        cmd->BeginRenderPass();

        result=render_task->Render(cmd);

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
