#include<hgl/graph/Renderer.h>
#include<hgl/graph/Scene.h>
#include<hgl/graph/VKCommandBuffer.h>

namespace hgl::graph
{
    Renderer::Renderer(IRenderTarget *rt)
    {
        render_target=rt;
        scene=nullptr;
        camera=nullptr;
        render_task=new RenderTask("DefaultRenderTask",rt);

        clear_color.Set(0,0,0,1);
    }

    Renderer::~Renderer()
    {
        delete render_task;
    }

    void Renderer::SetRenderTarget(IRenderTarget *rt)
    {
        if(render_target==rt)
            return;

        render_target=rt;

        render_task->Set(rt);
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
    }

    bool Renderer::RenderFrame()
    {
        if(!scene)
            return(false);

        SceneNode *root=scene->GetRootNode();

        if(!root)
            return(false);

        root->RefreshMatrix();

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
