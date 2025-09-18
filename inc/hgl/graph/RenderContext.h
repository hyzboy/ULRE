#pragma once

#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/camera/FrameCamera.h>
#include<hgl/graph/camera/CameraControl.h>
#include<hgl/graph/Ray.h>
#include<hgl/graph/geo/line/LineRenderManager.h>

namespace hgl::graph
{
    /** 渲染上下文，聚合与单帧渲染相关资源(可持续扩展) */
    class RenderContext
    {
        RenderFramework *rf = nullptr;
        IRenderTarget *render_target = nullptr;    ///< 当前渲染目标(用于viewport信息)
        Scene *scene = nullptr;    ///< 场景指针(不负责释放)
        FrameCamera *frame_camera = nullptr;    ///< 摄像机数据封装
        CameraControl *camera_control = nullptr;    ///< 摄像机控制(策略指针,不负责释放)
        LineRenderManager *line_render_mgr = nullptr;    ///< 线段渲染管理器

    public:

        RenderContext(RenderFramework *rf,IRenderTarget *rt);
        ~RenderContext();

        void            SetRenderTarget(IRenderTarget *rt);
        void            SetScene(Scene *s){ scene = s; }
        void            SetCameraControl(CameraControl *cc);

        // 每帧更新
        void            Tick(double delta);

        // 绑定描述符：摄像机 + 场景
        void            BindDescriptor(RenderCmdBuffer *cmd);

        // 访问器
        Scene *GetScene()const{ return scene; }
        FrameCamera *GetFrameCamera()const{ return frame_camera; }
        Camera *GetCamera()const{ return frame_camera ? frame_camera->GetCamera() : nullptr; }
        const CameraInfo *GetCameraInfo()const{ return frame_camera ? frame_camera->GetCameraInfo() : nullptr; }
        CameraControl *GetCameraControl()const{ return camera_control; }
        LineRenderManager *GetLineRenderManager()const{ return line_render_mgr; }
        const ViewportInfo *GetViewportInfo()const{ return render_target ? render_target->GetViewportInfo() : nullptr; }
        const VkExtent2D &GetExtent()const{ return render_target->GetExtent(); }
    };//class RenderContext
}//namesapce hgl::graph
