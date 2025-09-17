#include<hgl/graph/SceneRenderer.h>
#include<hgl/graph/Scene.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/camera/Camera.h>
#include<hgl/graph/geo/line/LineRenderManager.h>

namespace hgl::graph
{
    LineRenderManager *CreateLineRenderManager(RenderFramework *rf,IRenderTarget *);

    SceneRenderer::SceneRenderer(RenderFramework *rf,IRenderTarget *rt)
    {
        render_target=rt;
        scene=nullptr;
        camera_control=nullptr;

        render_task=new RenderTask("DefaultRenderTask",rt);

        clear_color.Set(0,0,0,1);

        line_render_manager = CreateLineRenderManager(rf,rt);
    }

    SceneRenderer::~SceneRenderer()
    {
        SAFE_CLEAR(line_render_manager)
        delete render_task;
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

        //if(scene)
        //{
        //    scene->Join(this);
        //}
    }

    void SceneRenderer::SetCameraControl(CameraControl *cc)
    {
        if(!scene||!cc)
            return;

        //if(camera)
        //{
        //    if(scene)
        //        camera->Unjoin(scene);

        //    camera->Unjoin(this);
        //}

        camera_control=cc;

        //if(camera)
        //{
        //    if(scene)
        //        camera->Unjoin(scene);

        //    camera->Join(this);
        //}

        render_task->SetCameraInfo(camera_control->GetCameraInfo());
    }
    
    bool SceneRenderer::RenderFrame()
    {
        if(!scene)
            return(false);

        SceneNode *root = scene->GetRootNode();

        if(!root)
            return(false);

        root->UpdateWorldTransform();

        if(camera_control)
        {
            camera_control->SetViewport(render_target->GetViewportInfo());

            camera_control->Refresh();
        }

        // 这里内部会将Scene tree展开成RenderCollector,而RenderCollector排序是需要CameraInfo的
        render_task->RebuildRenderList(root);

        bool result = false;

        RenderCmdBuffer *cmd = render_target->BeginRender();

        if(camera_control)
        {
            cmd->SetDescriptorBinding(camera_control->GetDescriptorBinding());
        }

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
