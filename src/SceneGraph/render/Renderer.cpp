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
        camera=nullptr;
        ubo_camera_info=rt->GetDevice()->CreateUBO<UBOCameraInfo>();
        render_task=new RenderTask("DefaultRenderTask",rt);

        clear_color.Set(0,0,0,1);
    }

    Renderer::~Renderer()
    {
        delete ubo_camera_info;
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
        render_task->SetCamera(camera);

        return(true);
    }

    void Renderer::SetCurScene(Scene *sw)
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

    void Renderer::SetCurCamera(Camera *c)
    {
        if(!scene||!c)
            return;

        //if(camera)
        //{
        //    if(scene)
        //        camera->Unjoin(scene);

        //    camera->Unjoin(this);
        //}

        camera=c;

        //if(camera)
        //{
        //    if(scene)
        //        camera->Unjoin(scene);

        //    camera->Join(this);
        //}

        render_task->SetCamera(camera);   //它要camera只是为了CPU端算远近
    }

    bool Renderer::RenderFrame()
    {
        if(!scene)
            return(false);

        SceneNode *root=scene->GetRootNode();

        if(!root)
            return(false);

        root->RefreshMatrix();

        if(camera)
        {
            RefreshCameraInfo(ubo_camera_info->data(),render_target->GetViewportInfo(),camera);
        }

        render_task->RebuildRenderList(root);

        bool result=false;

        graph::RenderCmdBuffer *cmd=render_target->BeginRender();

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
