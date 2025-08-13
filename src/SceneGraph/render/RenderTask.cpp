#include<hgl/graph/RenderTask.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKRenderTarget.h>

namespace hgl::graph
{
    RenderTask::RenderTask(const RenderTaskName &tn,IRenderTarget *rt,CameraInfo *ci)
    {
        task_name=tn;

        camera_info=ci;
        render_target=nullptr;
        render_list=nullptr;

        SetRenderTarget(rt);
    }

    RenderTask::~RenderTask()
    {
        SAFE_CLEAR(render_list)
    }

    bool RenderTask::SetRenderTarget(IRenderTarget *rt)
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

        if(!rt)
            return(false);

        if(!render_list)
        {
            render_list=new RenderList(rt->GetDevice());

            if(camera_info)
                render_list->SetCameraInfo(camera_info);
        }

        return(true);
    }

    void RenderTask::SetCameraInfo(CameraInfo *ci)
    {
        if(camera_info==ci)return;

        camera_info=ci;

        render_list->SetCameraInfo(ci);
    }

    bool RenderTask::RebuildRenderList(SceneNode *root)
    {
        if(!root)
            return(false);

        if(!render_list)
            return(false);

        if(!render_list->GetCameraInfo()&&camera_info)
            render_list->SetCameraInfo(camera_info);

        //记往不需要，也千万不要手动render_list->Clear，因为那会释放内存。再次使用时重新分配
        //render_list->Expend会自己复位所有数据，且并不会释放内存
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
