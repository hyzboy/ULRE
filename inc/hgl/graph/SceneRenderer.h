#pragma once

#include<hgl/graph/RenderTask.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/camera/CameraControl.h>
#include<hgl/type/Map.h>

namespace hgl::graph
{
    class Scene;
    class CameraControl;
    class LineRenderManager;

    using RenderTaskNameMap=Map<RenderTaskName,RenderTask *>;

    /**
    * 场景渲染器
    */
    class SceneRenderer
    {
        IRenderTarget * render_target = nullptr;
        Scene *         scene = nullptr;

        CameraControl * camera_control = nullptr;

        //RenderTaskNameMap static_render_task_list;                              ///<静态渲染任务列表
        //RenderTaskNameMap dynamic_render_task_list;                             ///<动态渲染任务列表

        RenderTask *    render_task=nullptr;                                      ///<当前渲染任务

    protected: //线条渲染管理纯只是画线条，并不是资源管理器,所以和上面的放开

        LineRenderManager *line_render_manager = nullptr;

    protected:

        Color4f clear_color;                                                        ///<清屏颜色

        bool render_state_dirty=false;

    public:

                RenderPass *GetRenderPass   (){return render_target->GetRenderPass();}      ///<取得当前渲染器RenderPass

        const   VkExtent2D &GetExtent       ()const{return render_target->GetExtent();}     ///<取得当前渲染器画面尺寸

                Scene *     GetScene        ()const{return scene;}                          ///<获取场景世界
                Camera *    GetCamera       ()const{return camera_control->GetCamera();}                         ///<获取当前相机

                LineRenderManager *GetLineRenderManager()const{ return line_render_manager; }   ///<取得线条渲染管理器

    public:

        SceneRenderer(RenderFramework *,IRenderTarget *);
        virtual ~SceneRenderer();

        bool SetRenderTarget(IRenderTarget *);
        void SetScene(Scene *);
        void SetCameraControl(CameraControl *);

        void SetClearColor(const Color4f &c){clear_color=c;}

        bool BeginRender();
        bool RenderFrame();                                                                 ///<重新重成这一帧的CommandList
        bool Submit();                                                                      ///<提交CommandList到GPU
    };//class SceneRenderer
}//namespace hgl::graph
