#include<hgl/graph/RenderTask.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKRenderTarget.h>

namespace hgl::graph
{
    RenderTask::RenderTask(const RenderTaskName &tn,IRenderTarget *rt,Camera *c)
    {
        task_name=tn;

        camera=c;
        render_target=nullptr;
        render_list=nullptr;

        Set(rt);
    }

    RenderTask::~RenderTask()
    {
        SAFE_CLEAR(render_list)
    }

    bool RenderTask::Set(IRenderTarget *rt)
    {
        if(render_target)
        {
            if(render_target->GetDevice()!=rt->GetDevice())
            {
                //换Device是不允许的，当然这一般也不可能
                return(false);
            }
        }

        render_target=rt;

        if(!render_list)
        {
            render_list=new RenderList(rt->GetDevice());
        }

        return(true);
    }

    void RenderTask::Restart()
    {
        if(!render_list)
            return;

        render_list->Clear();
    }

    bool RenderTask::RebuildRenderList(SceneNode *root)
    {
        if(!root)
            return(false);

        if(!render_list)
            return(false);

        render_list->Clear();
        render_list->Expend(root);

        return(true);
    }

    bool RenderTask::IsEmpty()const
    {
        if(!render_list)
            return(true);

        return render_list->IsEmpty();
    }

    bool RenderTask::Render(RenderCmdBuffer *cmd)
    {
        if(!cmd)
            return(false);

        if(!render_target)
            return(false);

        if(!render_list)
            return(false);

        if(render_list->IsEmpty())
            return(false);

        render_list->Render(cmd);
        return(true);
    }
}//namespace hgl::graph
