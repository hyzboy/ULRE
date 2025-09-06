#include<hgl/graph/camera/CameraControl.h>
#include<hgl/graph/VKDescriptorSetType.h>
#include<hgl/graph/VKDescriptorBindingManage.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/Ray.h>

namespace hgl::graph
{
    CameraControl::CameraControl(ViewportInfo *v,Camera *c,UBOCameraInfo *ci)
    {
        vi=v;
        camera=c;
        ubo_camera_info=ci;
        camera_info=ubo_camera_info->data();

        desc_binding_camera=new DescriptorBinding(DescriptorSetType::Camera);
        desc_binding_camera->AddUBO(ubo_camera_info);
    }

    CameraControl::~CameraControl()
    {
        delete desc_binding_camera;
        delete ubo_camera_info;
    }

    bool CameraControl::SetMouseRay(Ray *ray,const Vector2i &mouse_coord)
    {
        if(!ray||!camera_info||!vi)return(false);

        ray->Set(mouse_coord,camera_info,vi);

        return(true);
    }
}//namespace hgl::graph
