#include<hgl/graph/Renderer.h>
#include<hgl/graph/Scene.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/Camera.h>

namespace hgl::graph
{
    Renderer::Renderer(IRenderTarget *rt)
    {
        render_target=rt;
        scene=nullptr;
        camera_control=nullptr;

        render_task=new RenderTask("DefaultRenderTask",rt);

        clear_color.Set(0,0,0,1);
    }

    Renderer::~Renderer()
    {
        delete render_task;
    }

    bool Renderer::SetRenderTarget(IRenderTarget *rt)
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

    void Renderer::SetScene(Scene *sw)
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

    void Renderer::SetCameraControl(CameraControl *cc)
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

    bool Renderer::RenderFrame()
    {
        if(!scene)
            return(false);

        SceneNode *root=scene->GetRootNode();

        if(!root)
            return(false);

        root->RefreshMatrix();

        if(camera_control)
        {
            camera_control->SetViewport(render_target->GetViewportInfo());

            camera_control->Refresh();
        }

        // 这里内部会将Scene tree展开成RenderList,而RenderList排序是需要CameraInfo的
        render_task->RebuildRenderList(root);

        bool result=false;

        graph::RenderCmdBuffer *cmd=render_target->BeginRender();

        if(camera_control)
        {
            cmd->SetDescriptorBinding(camera_control->GetDescriptorBinding());
        }

        cmd->SetClearColor(0,clear_color);

        cmd->BeginRenderPass();
            result=render_task->Render(cmd);
        cmd->EndRenderPass();

        render_target->EndRender();

        build_frame=result;

        return(result);
    }

    bool Renderer::Submit()
    {
        if(!render_target)
            return(false);

        if(!build_frame)
            return(false);

        return render_target->Submit();
    }
}//namespace hgl::graph
