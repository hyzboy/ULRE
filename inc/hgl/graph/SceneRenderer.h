#pragma once

#include<hgl/graph/RenderTask.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/Scene.h>
#include<hgl/type/Map.h>
#include<hgl/graph/geo/line/LineRenderManager.h>
#include<hgl/graph/camera/CameraControl.h>
#include<hgl/graph/camera/FrameCamera.h>

namespace hgl::graph
{
    class Scene;

    using RenderTaskNameMap=Map<RenderTaskName,RenderTask *>;

    /**
    * 场景渲染器
    */
    class SceneRenderer
    {
        IRenderTarget *     render_target       = nullptr;
        Scene *             scene               = nullptr;

        FrameCamera *       frame_camera        = nullptr;   ///< 当前帧摄像机包装
        CameraControl *     camera_control      = nullptr;   ///< 摄像机控制

        RenderTask *        render_task         = nullptr;   ///< 当前渲染任务

        LineRenderManager * line_render_manager = nullptr;   ///< 线段渲染管理器(移出Scene)

    protected:

        Color4f clear_color;                                                    ///<清屏颜色

        bool render_state_dirty=false;

    public:

                RenderPass *        GetRenderPass       ()      {return render_target->GetRenderPass();}
        const   ViewportInfo *      GetViewportInfo     ()const {return render_target->GetViewportInfo();}
        const   VkExtent2D &        GetExtent           ()const {return render_target->GetExtent();}

                Scene *             GetScene            ()const {return scene;}
                Camera *            GetCamera           ()const {return frame_camera?frame_camera->GetCamera():nullptr;}
        const   CameraInfo *        GetCameraInfo       ()const {return frame_camera?frame_camera->GetCameraInfo():nullptr;}
                CameraControl *     GetCameraControl    ()const {return camera_control;}
                LineRenderManager * GetLineRenderManager()const {return line_render_manager;}

    public:

        SceneRenderer(RenderFramework *,IRenderTarget *);
        virtual ~SceneRenderer();

        bool SetRenderTarget(IRenderTarget *);
        void SetScene(Scene *);
        void SetCameraControl(CameraControl *);

        void SetClearColor(const Color4f &c){clear_color=c;}

        void Tick(double);

        bool BeginRender();
        bool RenderFrame();
        bool Submit();
    };//class SceneRenderer
}//namespace hgl::graph
