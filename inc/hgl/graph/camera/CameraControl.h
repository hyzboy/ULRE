#pragma once

#include<hgl/graph/camera/Camera.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/type/IDName.h>
#include<hgl/io/event/WindowEvent.h>

namespace hgl::graph
{
    struct Ray;

    using UBOCameraInfo=UBOInstance<CameraInfo>;

    HGL_DEFINE_IDNAME(CameraControlIDName,   char)

    class CameraControl:public io::WindowEvent
    {
    protected:

        const   ViewportInfo *  vi = nullptr;
                Camera *        camera = nullptr;
                CameraInfo *    camera_info = nullptr;

    public:

        virtual const CameraControlIDName GetControlName()const=0;

    public:

        CameraControl()=default;
        virtual ~CameraControl()=default;

        void SetViewport(const ViewportInfo *i)
        {
            vi=i;
        }

        void SetCamera(Camera *c,CameraInfo *ci)
        {
            camera=c;
            camera_info=ci;
        }

        void SetPosition(const Vector3f &p)
        {
            if(camera)
                camera->pos=p;
        }

        virtual void SetTarget(const Vector3f &t)=0;

        void ZoomFOV(int adjust);

        const   ViewportInfo *  GetViewportInfo ()const{return vi;}
                Camera *        GetCamera       ()const{return camera;}
                CameraInfo *    GetCameraInfo   ()const{return camera_info;}

        virtual void Refresh()=0;

    public:

        bool SetMouseRay(Ray *,const Vector2i &);

        const Vector3f LocalToViewSpace(const Matrix4f &l2w,const Vector3f &local_pos)const
        {
            if(!vi||!camera_info)return(Vector3f(0,0,0));

            const Vector4f clip_pos=camera_info->LocalToViewSpace(l2w,local_pos);

            if(clip_pos.w==0.0f)
                return(Vector3f(0,0,0));

            return Vector3f(clip_pos.x/clip_pos.w,clip_pos.y/clip_pos.w,clip_pos.z/clip_pos.w);
        }

        const Vector2i WorldPositionToScreen(const Vector3f &wp)const                       ///<世界坐标转换为屏幕坐标
        {
            if(!vi||!camera_info)return(Vector2i(0,0));

            return ProjectToScreen(wp,camera_info->view,camera_info->projection,vi->GetViewportWidth(),vi->GetViewportHeight());
        }

        const Vector3f ScreenPositionToWorld(const Vector2i &sp)const                       ///<屏幕坐标转换为世界坐标
        {
            if(!vi||!camera_info)return(Vector3f(0,0,0));

            return UnProjectToWorld(sp,camera_info->view,camera_info->projection,vi->GetViewportWidth(),vi->GetViewportHeight());
        }
    };//class CameraControl
}//namespace hgl::graph
