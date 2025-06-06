#pragma once

#include<hgl/graph/RenderTask.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/type/Map.h>

namespace hgl::graph
{
    class Scene;

    using RenderTaskNameMap=Map<RenderTaskName,RenderTask *>;

    using UBOCameraInfo=DeviceBufferMap<CameraInfo>;

    /**
    * 渲染器
    */
    class Renderer
    {
        IRenderTarget *render_target;
        Scene *scene;

        Camera *camera;

        UBOCameraInfo *ubo_camera_info;

        //RenderTaskNameMap static_render_task_list;                              ///<静态渲染任务列表
        //RenderTaskNameMap dynamic_render_task_list;                             ///<动态渲染任务列表

        RenderTask *render_task;                                                ///<当前渲染任务

        Color4f clear_color;                                                    ///<清屏颜色

        bool build_frame=false;

    public:

                RenderPass *GetRenderPass   (){return render_target->GetRenderPass();}      ///<取得当前渲染器RenderPass

        const   VkExtent2D &GetExtent       ()const{return render_target->GetExtent();}     ///<取得当前渲染器画面尺寸

                Scene *     GetScene        ()const{return scene;}                          ///<获取场景世界
                Camera *    GetCurCamera    ()const{return camera;}                         ///<获取当前相机

    public:

        Renderer(IRenderTarget *);
        virtual ~Renderer();

        bool SetRenderTarget(IRenderTarget *);
        void SetCurScene(Scene *);
        void SetCurCamera(Camera *);

        void SetClearColor(const Color4f &c){clear_color=c;}

        bool RenderFrame();                                                                 ///<重新重成这一帧的CommandList
        bool Submit();                                                                      ///<提交CommandList到GPU
    };//class Renderer
}//namespace hgl::graph
