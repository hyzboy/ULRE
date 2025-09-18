#pragma once

#include<hgl/graph/camera/Camera.h>
#include<hgl/graph/VKDescriptorBindingManage.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/Ray.h>
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

        bool SetMouseRay(Ray *ray,const Vector2i &mouse_coord)
        {
            if(!ray || !ubo_camera_info || !viewport_info)return(false);

            ray->Set(mouse_coord,ubo_camera_info->data(),viewport_info);

            return(true);
        }

        const Vector3f LocalToViewSpace(const Matrix4f &l2w,const Vector3f &local_pos)const
        {
            if(!viewport_info || !ubo_camera_info)return(Vector3f(0,0,0));

            const Vector4f clip_pos = ubo_camera_info->data()->LocalToViewSpace(l2w,local_pos);

            if(clip_pos.w == 0.0f)
                return(Vector3f(0,0,0));

            return Vector3f(clip_pos.x / clip_pos.w,clip_pos.y / clip_pos.w,clip_pos.z / clip_pos.w);
        }

        const Vector2i WorldPositionToScreen(const Vector3f &wp)const                       ///<世界坐标转换为屏幕坐标
        {
            if(!viewport_info || !ubo_camera_info)return(Vector2i(0,0));

            return ProjectToScreen(wp,ubo_camera_info->data()->view,ubo_camera_info->data()->projection,viewport_info->GetViewportWidth(),viewport_info->GetViewportHeight());
        }

        const Vector3f ScreenPositionToWorld(const Vector2i &sp)const                       ///<屏幕坐标转换为世界坐标
        {
            if(!viewport_info || !ubo_camera_info)return(Vector3f(0,0,0));

            return UnProjectToWorld(sp,ubo_camera_info->data()->view,ubo_camera_info->data()->projection,viewport_info->GetViewportWidth(),viewport_info->GetViewportHeight());
        }
    };//class FrameCamera
}//namespace hgl::graph
