#pragma once

#include<hgl/graph/RenderTask.h>
#include<hgl/type/Map.h>

namespace hgl::graph
{
    class SceneWorld;

    using RenderTaskNameMap=Map<RenderTaskName,RenderTask *>;

    /**
    * 渲染器
    * 管理相对的所有的渲染资源，包括场景、相机、渲染目标、渲染任务等
    */
    class Renderer
    {
        SceneWorld *world;

        RenderTaskNameMap static_render_task_list;                              ///<静态渲染任务列表
        RenderTaskNameMap dynamic_render_task_list;                             ///<动态渲染任务列表

    public:


    };//class Renderer
}//namespace hgl::graph
