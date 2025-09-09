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

        IRenderTarget *     render_target;
        RenderCollector *   render_collector;
        CameraInfo *        camera_info;

    public:

        const   RenderTaskName &    GetName         ()const;

                IRenderTarget *     GetRenderTarget ()const{return render_target;}
                RenderCollector *   GetRenderList   ()const{return render_collector;}
                CameraInfo *        GetCameraInfo   ()const{return camera_info;}

    public:

        RenderTask(const RenderTaskName &tn,IRenderTarget *rt=nullptr,CameraInfo *ci=nullptr);

        virtual ~RenderTask();

        bool SetRenderTarget(IRenderTarget *);
        void SetCameraInfo(CameraInfo *);

        bool RebuildRenderList(SceneNode *);

        bool IsEmpty()const;                        ///<是否是空的，不可渲染或是没啥可渲染的

        bool Render(RenderCmdBuffer *);
    };//class RenderTask
}//namespace hgl::graph
