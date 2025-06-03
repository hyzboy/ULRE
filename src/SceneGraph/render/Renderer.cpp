#include<hgl/graph/Renderer.h>
#include<hgl/graph/SceneWorld.h>
#include<hgl/graph/VKCommandBuffer.h>

namespace hgl::graph
{
    Renderer::Renderer(IRenderTarget *rt)
    {
        render_target=rt;
        world=nullptr;
        render_task=new RenderTask("TempRenderTask");
    }

    Renderer::~Renderer()
    {
        delete render_task;
        delete world;
    }

    void Renderer::SetCurWorld(Scene *sw)
    {
        if(world==sw)
            return;

        //if(world)
        //{
        //    world->Unjoin(this);
        //}

        world=sw;

        //if(world)
        //{
        //    world->Join(this);
        //}
    }

    void Renderer::SetCurCamera(Camera *c)
    {
        if(!world||!c)
            return;

        //if(camera)
        //{
        //    if(world)
        //        camera->Unjoin(world);

        //    camera->Unjoin(this);
        //}

        camera=c;

        //if(camera)
        //{
        //    if(world)
        //        camera->Unjoin(world);

        //    camera->Join(this);
        //}
    }

    bool Renderer::RenderFrame(RenderCmdBuffer *cmd)
    {
        if(!world)
            return(false);

        SceneNode *root=world->GetRootNode();

        if(!root)
            return(false);

        root->RefreshMatrix();

        render_task->RebuildRenderList(root);

        bool result=false;

        cmd->BeginRenderPass();
            result=render_task->Render(cmd);
        cmd->EndRenderPass();

        return(result);
    }
}//namespace hgl::graph
