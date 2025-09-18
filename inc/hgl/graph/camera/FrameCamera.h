#pragma once

#include<hgl/graph/camera/Camera.h>
#include<hgl/graph/VKDescriptorBindingManage.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/mtl/UBOCommon.h>

namespace hgl::graph
{
    class RenderFramework;   // forward

    using UBOCameraInfo = UBOInstance<CameraInfo>;      ///< 摄像机信息UBO类型

    /**
    * 封装当前帧摄像机所有相关数据(摄像机本体/UBO/DescriptorBinding)，统一管理
    */
    class FrameCamera
    {
        Camera                  camera;                 ///< 摄像机数据(值类型)
        UBOCameraInfo *         ubo_camera_info = nullptr;  ///< 摄像机信息UBO
        DescriptorBinding *     desc_binding    = nullptr;  ///< 摄像机描述符绑定(DescriptorSetType::Camera)
        const ViewportInfo *    viewport_info   = nullptr;  ///< 视口信息(来自RenderTarget)

    public:
        FrameCamera(RenderFramework *rf);   ///< 构造(实现见cpp)

        ~FrameCamera()
        {
            SAFE_CLEAR(desc_binding);
            SAFE_CLEAR(ubo_camera_info);
        }

        void SetViewportInfo(const ViewportInfo *vi){ viewport_info = vi; }
        const ViewportInfo *GetViewportInfo()const { return viewport_info; }

        Camera *GetCamera(){ return &camera; }
        const Camera *GetCamera() const { return &camera; }

        CameraInfo *GetCameraInfo(){ return ubo_camera_info?ubo_camera_info->data():nullptr; }
        const CameraInfo *GetCameraInfo() const { return ubo_camera_info?ubo_camera_info->data():nullptr; }

        DescriptorBinding *GetDescriptorBinding(){ return desc_binding; }

        void UpdateUBO(){ if(ubo_camera_info) ubo_camera_info->Update(); }
    };//class FrameCamera
}//namespace hgl::graph
