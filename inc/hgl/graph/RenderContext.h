#pragma once

#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/camera/Camera.h>
#include<hgl/graph/camera/CameraControl.h>
#include<hgl/graph/VKDescriptorBindingManage.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/Ray.h>
#include<hgl/graph/geo/line/LineRenderManager.h>

namespace hgl::graph
{
    using UBOCameraInfo = UBOInstance<CameraInfo>;      ///< 摄像机信息UBO类型

    /** 渲染上下文，聚合与单帧渲染相关资源(可持续扩展) */
    class RenderContext
    {
        RenderFramework *rf = nullptr;
        IRenderTarget *render_target = nullptr;    ///< 当前渲染目标(用于viewport信息)
        Scene *scene = nullptr;                    ///< 场景指针(不负责释放)
        CameraControl *camera_control = nullptr;   ///< 摄像机控制(策略指针,不负责释放)
        LineRenderManager *line_render_mgr = nullptr;    ///< 线段渲染管理器

        Camera              camera;                 ///< 摄像机数据(值类型)
        UBOCameraInfo *     ubo_camera_info = nullptr;   ///< 摄像机信息UBO
        DescriptorBinding * camera_desc_binding    = nullptr;   ///< 摄像机描述符绑定(DescriptorSetType::Camera)

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

        Camera *GetCamera(){ return &camera; }
        const Camera *GetCamera() const { return &camera; }

        CameraInfo *GetCameraInfo(){ return ubo_camera_info?ubo_camera_info->data():nullptr; }
        const CameraInfo *GetCameraInfo() const { return ubo_camera_info?ubo_camera_info->data():nullptr; }

        DescriptorBinding *GetCameraDescriptor(){ return camera_desc_binding; }

        void UpdateCameraUBO(){ if(ubo_camera_info) ubo_camera_info->Update(); }

        bool SetMouseRay(Ray *ray,const Vector2i &mouse_coord)
        {
            if(!ray || !ubo_camera_info || !render_target) return false;
            const ViewportInfo *vi = render_target->GetViewportInfo();
            if(!vi) return false;
            ray->Set(mouse_coord,ubo_camera_info->data(),vi);
            return true;
        }

        const Vector3f LocalToViewSpace(const Matrix4f &l2w,const Vector3f &local_pos)const
        {
            const ViewportInfo *vi = render_target?render_target->GetViewportInfo():nullptr;
            if(!vi || !ubo_camera_info) return Vector3f(0,0,0);

            const Vector4f clip_pos = ubo_camera_info->data()->LocalToViewSpace(l2w,local_pos);
            if(clip_pos.w == 0.0f) return Vector3f(0,0,0);
            return Vector3f(clip_pos.x/clip_pos.w,clip_pos.y/clip_pos.w,clip_pos.z/clip_pos.w);
        }

        const Vector2i WorldPositionToScreen(const Vector3f &wp)const   ///<世界坐标转换为屏幕坐标
        {
            const ViewportInfo *vi = render_target?render_target->GetViewportInfo():nullptr;
            if(!vi || !ubo_camera_info) return Vector2i(0,0);
            return ProjectToScreen(wp,ubo_camera_info->data()->view,ubo_camera_info->data()->projection,vi->GetViewportWidth(),vi->GetViewportHeight());
        }

        const Vector3f ScreenPositionToWorld(const Vector2i &sp)const   ///<屏幕坐标转换为世界坐标
        {
            const ViewportInfo *vi = render_target?render_target->GetViewportInfo():nullptr;
            if(!vi || !ubo_camera_info) return Vector3f(0,0,0);
            return UnProjectToWorld(sp,ubo_camera_info->data()->view,ubo_camera_info->data()->projection,vi->GetViewportWidth(),vi->GetViewportHeight());
        }

        // 访问器
        Scene *GetScene()const{ return scene; }
        CameraControl *GetCameraControl()const{ return camera_control; }
        LineRenderManager *GetLineRenderManager()const{ return line_render_mgr; }
        const ViewportInfo *GetViewportInfo()const{ return render_target ? render_target->GetViewportInfo() : nullptr; }
        const VkExtent2D &GetExtent()const{ return render_target->GetExtent(); }
    };//class RenderContext
}//namesapce hgl::graph
