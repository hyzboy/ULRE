#pragma once

#include<hgl/graph/camera/Camera.h>
#include<hgl/graph/Ray.h>

namespace hgl::graph
{
    class RenderContext
    {
        const ViewportInfo *  vi          = nullptr;
        const Camera *        camera      = nullptr;
        const CameraInfo *    camera_info = nullptr;

        uint64          frame_count = 0;    //帧计数器

        double          time        = 0;    //当前时间(单位秒)

    private:

        friend class SceneRenderer;

        void SetViewportInfo(const ViewportInfo *v){ vi = v; }
        void SetCamera(const Camera *c,const CameraInfo *ci){ camera = c;camera_info = ci; }

    public:

        const ViewportInfo *GetViewportInfo ()const{ return vi; }
        const Camera *      GetCamera       ()const{ return camera; }
        const CameraInfo *  GetCameraInfo   ()const{ return camera_info; }

        uint64              GetFrameCount   ()const{ return frame_count; }
        double              GetTime         ()const{ return time; }

    public:

        RenderContext()=default;
        ~RenderContext()=default;

        void NextFrame(double delta)
        {
            ++frame_count;
            time+=delta;
        }

        bool SetMouseRay(Ray *ray,const Vector2i &mouse_coord)
        {
            if(!ray || !camera_info || !vi)return(false);

            ray->Set(mouse_coord,camera_info,vi);

            return(true);
        }

        const Vector3f LocalToViewSpace(const Matrix4f &l2w,const Vector3f &local_pos)const
        {
            if(!vi || !camera_info)return(Vector3f(0,0,0));

            const Vector4f clip_pos = camera_info->LocalToViewSpace(l2w,local_pos);

            if(clip_pos.w == 0.0f)
                return(Vector3f(0,0,0));

            return Vector3f(clip_pos.x / clip_pos.w,clip_pos.y / clip_pos.w,clip_pos.z / clip_pos.w);
        }

        const Vector2i WorldPositionToScreen(const Vector3f &wp)const                       ///<世界坐标转换为屏幕坐标
        {
            if(!vi || !camera_info)return(Vector2i(0,0));

            return ProjectToScreen(wp,camera_info->view,camera_info->projection,vi->GetViewportWidth(),vi->GetViewportHeight());
        }

        const Vector3f ScreenPositionToWorld(const Vector2i &sp)const                       ///<屏幕坐标转换为世界坐标
        {
            if(!vi || !camera_info)return(Vector3f(0,0,0));

            return UnProjectToWorld(sp,camera_info->view,camera_info->projection,vi->GetViewportWidth(),vi->GetViewportHeight());
        }
    };//class RenderContext
}//namesapce hgl::graph
