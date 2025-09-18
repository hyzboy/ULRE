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
    };//class CameraControl
}//namespace hgl::graph
