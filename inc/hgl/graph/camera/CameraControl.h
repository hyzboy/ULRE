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

        ViewportInfo *vi;
        Camera *camera;

        UBOCameraInfo *ubo_camera_info;
        CameraInfo *camera_info;
        DescriptorBinding *desc_binding_camera;

    public:

        virtual const CameraControlIDName GetControlName()const=0;

    public:

        CameraControl(ViewportInfo *v,Camera *c,UBOCameraInfo *ci);

        virtual ~CameraControl();

        void SetViewport(ViewportInfo *i)
        {
            vi=i;
        }

        void SetCamera(Camera *c)
        {
            camera=c;
        }

        void SetPosition(const Vector3f &p)
        {
            if(camera)
                camera->pos=p;
        }

        virtual void SetTarget(const Vector3f &t)=0;

        void ZoomFOV(int adjust)
        {
            constexpr float MinFOV=10;
            constexpr float MaxFOV=180;

            camera->Yfov+=float(adjust)/10.0f;

            if(adjust<0&&camera->Yfov<MinFOV)camera->Yfov=MinFOV;else
            if(adjust>0&&camera->Yfov>MaxFOV)camera->Yfov=MaxFOV;
        }

        ViewportInfo *      GetViewportInfo     (){return vi;}
        Camera *            GetCamera           (){return camera;}
        CameraInfo *        GetCameraInfo       (){return camera_info;}

        DescriptorBinding * GetDescriptorBinding(){return desc_binding_camera;}

        virtual void Refresh()=0;

    public:

        bool SetMouseRay(Ray *,const Vector2i &);

        /**
        * 本地坐标到观察坐标
        */
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
