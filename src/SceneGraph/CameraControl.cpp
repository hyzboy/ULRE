#include<hgl/graph/camera/CameraControl.h>
#include<hgl/graph/VKDescriptorSetType.h>
#include<hgl/graph/VKDescriptorBindingManage.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/Ray.h>

namespace hgl::graph
{
    void CameraControl::ZoomFOV(int adjust)
    {
        if(!camera)
            return;

        constexpr float MinFOV = 10;
        constexpr float MaxFOV = 180;

        camera->Yfov += float(adjust) / 10.0f;

        if(adjust < 0 && camera->Yfov < MinFOV)camera->Yfov = MinFOV;else
        if(adjust > 0 && camera->Yfov > MaxFOV)camera->Yfov = MaxFOV;
    }

    bool CameraControl::SetMouseRay(Ray *ray,const Vector2i &mouse_coord)
    {
        if(!ray||!camera_info||!vi)return(false);

        ray->Set(mouse_coord,camera_info,vi);

        return(true);
    }
}//namespace hgl::graph
