#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/IDName.h>

namespace hgl::graph
{
    class SceneNode;
    class SceneWorld;
    class Camera;
    class IRenderTarget;
    class RenderList;

    HGL_DEFINE_IDNAME(RenderTaskName,char)

    /**
    * 最终的具体渲染任务
    */
    class RenderTask
    {
        RenderTaskName  task_name;

        RenderList *    render_list;
        IRenderTarget * render_target;
        Camera *        camera;

    public:

        const   RenderTaskName &GetName         ()const;

                IRenderTarget * GetRenderTarget ()const{return render_target;}
                RenderList *    GetRenderList   ()const{return render_list;}
                Camera *        GetCamera       ()const{return camera;}

    public:

        RenderTask(const RenderTaskName &tn)
        {
            task_name=tn;

            render_list=nullptr;
            render_target=nullptr;
            camera=nullptr;
        }

        RenderTask(const RenderTaskName &tn,RenderList *rl,IRenderTarget *rt=nullptr,Camera *c=nullptr)
        {
            task_name=tn;

            render_list=rl;
            render_target=rt;
            camera=c;
        }

        virtual ~RenderTask();

        void Set(IRenderTarget *rt){render_target=rt;}
        void Set(Camera *c){camera=c;}

        void Restart();                             ///<复位数据，清空渲染列表

        void MakeRenderList(SceneNode *);

        bool IsEmpty()const;                        ///<是否是空的，不可渲染或是没啥可渲染的

        bool Render(RenderCmdBuffer *);
    };//class RenderTask
}//namespace hgl::graph
