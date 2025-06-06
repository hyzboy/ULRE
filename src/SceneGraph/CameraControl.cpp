#include<hgl/graph/CameraControl.h>
#include<hgl/graph/VKDescriptorSetType.h>
#include<hgl/graph/VKDescriptorBindingManage.h>
#include<hgl/graph/mtl/UBOCommon.h>

namespace hgl::graph
{
    CameraControl::CameraControl(ViewportInfo *v,Camera *c,UBOCameraInfo *ci)
    {
        vi=v;
        camera=c;
        ubo_camera_info=ci;
        camera_info=ubo_camera_info->data();

        desc_binding_camera=new DescriptorBinding(DescriptorSetType::Camera);
        desc_binding_camera->AddUBO(mtl::SBS_CameraInfo.name,ubo_camera_info);
    }

    CameraControl::~CameraControl()
    {
        delete desc_binding_camera;
        delete ubo_camera_info;
    }
}//namespace hgl::graph
