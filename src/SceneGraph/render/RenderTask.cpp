#include<hgl/graph/RenderTask.h>
#include<hgl/graph/RenderList.h>

namespace hgl::graph
{
    RenderTask::~RenderTask()
    {
        SAFE_CLEAR(render_list)
    }

    void RenderTask::Restart()
    {
        if(!render_list)
            return;

        render_list->Clear();
    }

    void RenderTask::MakeRenderList(SceneNode *root)
    {
        if(!root)
            return;

        if(!render_list)
            return;

        render_list->Clear();
        render_list->Expend(root);
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

        if(!render_list)
            return(false);

        if(render_list->IsEmpty())
            return(false);

        render_list->Render(cmd);
        return(true);
    }
}//namespace hgl::graph
