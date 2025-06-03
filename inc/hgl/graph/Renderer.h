#pragma once

#include<hgl/graph/RenderTask.h>
#include<hgl/type/Map.h>

namespace hgl::graph
{
    class Scene;

    using RenderTaskNameMap=Map<RenderTaskName,RenderTask *>;

    /**
    * 渲染器
    */
    class Renderer
    {
        IRenderTarget *render_target;
        Scene *world;

        Camera *camera;

        //RenderTaskNameMap static_render_task_list;                              ///<静态渲染任务列表
        //RenderTaskNameMap dynamic_render_task_list;                             ///<动态渲染任务列表

        RenderTask *render_task;                                                ///<当前渲染任务

    protected:



    public:

        Scene *GetScene   () const { return world; }                  ///<获取场景世界
        Camera *    GetCurCamera    () const { return camera; }                 ///<获取当前相机

    public:

        Renderer(IRenderTarget *);
        virtual ~Renderer();

        void SetCurWorld(Scene *);
        void SetCurCamera(Camera *);

        bool RenderFrame(RenderCmdBuffer *);
    };//class Renderer
}//namespace hgl::graph
