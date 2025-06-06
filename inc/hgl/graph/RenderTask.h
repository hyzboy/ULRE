#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/IDName.h>

namespace hgl::graph
{
    HGL_DEFINE_IDNAME(RenderTaskName,char)

    /**
    * 最终的具体渲染任务
    */
    class RenderTask
    {
        RenderTaskName  task_name;

        IRenderTarget * render_target;
        RenderList *    render_list;
        Camera *        camera;

    public:

        const   RenderTaskName &GetName         ()const;

                IRenderTarget * GetRenderTarget ()const{return render_target;}
                RenderList *    GetRenderList   ()const{return render_list;}
                Camera *        GetCamera       ()const{return camera;}

    public:

        RenderTask(const RenderTaskName &tn,IRenderTarget *rt=nullptr,Camera *c=nullptr);

        virtual ~RenderTask();

        bool SetRenderTarget(IRenderTarget *rt);
        void SetCamera(Camera *c){camera=c;}

        bool RebuildRenderList(SceneNode *);

        bool IsEmpty()const;                        ///<是否是空的，不可渲染或是没啥可渲染的

        bool Render(RenderCmdBuffer *);
    };//class RenderTask
}//namespace hgl::graph
