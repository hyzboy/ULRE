#pragma once

#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/camera/Camera.h>
#include<hgl/graph/camera/CameraControl.h>
#include<hgl/graph/VKDescriptorBindingManage.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/math/geometry/Ray.h>
#include<hgl/graph/geo/line/LineRenderManager.h>

namespace hgl::graph
{
    using UBOCameraInfo = UBOInstance<CameraInfo>;      ///< 摄像机信息UBO类型

    /** 渲染上下文，聚合与单帧渲染相关资源(可持续扩展) */
    class RenderContext
    {
                RenderFramework *   rf                  = nullptr;
                IRenderTarget *     render_target       = nullptr;  ///< 当前渲染目标
        const   ViewportInfo *      viewport_info       = nullptr;  ///< 缓存视口
                World *             world               = nullptr;  ///< 世界指针(不负责释放)
                CameraControl *     camera_control      = nullptr;  ///< 摄像机控制(策略指针,不负责释放)
                LineRenderManager * line_render_mgr     = nullptr;  ///< 线段渲染管理器

                Camera              camera;                         ///< 摄像机数据(值类型)
                UBOCameraInfo *     ubo_camera_info     = nullptr;  ///< 摄像机信息UBO
                DescriptorBinding * camera_desc_binding = nullptr;  ///< 摄像机描述符绑定(DescriptorSetType::Camera)

    protected:

        void UpdateCamera();

    public:

        const   ViewportInfo *      GetViewportInfo     ()const { return viewport_info; }
        const   Vector2u &    GetViewportSize     ()const { return viewport_info->GetViewport(); }
        const   VkExtent2D &        GetExtent           ()const { return render_target->GetExtent(); }

                Camera *            GetCamera           ()      { return &camera; }
        const   Camera *            GetCamera           ()const { return &camera; }

                CameraInfo *        GetCameraInfo       ()      { return ubo_camera_info ? ubo_camera_info->data() : nullptr; }
        const   CameraInfo *        GetCameraInfo       ()const { return ubo_camera_info ? ubo_camera_info->data() : nullptr; }
                CameraControl *     GetCameraControl    ()const { return camera_control; }

                World *             GetWorld            ()const { return world; }
                LineRenderManager * GetLineRenderManager()const { return line_render_mgr; }

    public:

        RenderContext(RenderFramework *rf,IRenderTarget *rt);
        ~RenderContext();

        void SetRenderTarget(IRenderTarget *rt);
        void SetWorld(World *s){ world = s; }
        void SetCameraControl(CameraControl *cc);

    public:

        void Tick(double delta);

        void BindDescriptor(RenderCmdBuffer *cmd);   ///< 绑定描述符：摄像机 + 场景
    };//class RenderContext
}//namesapce hgl::graph
