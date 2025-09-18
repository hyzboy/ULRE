#include<hgl/graph/camera/FrameCamera.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/mtl/UBOCommon.h>

namespace hgl::graph
{
    FrameCamera::FrameCamera(RenderFramework *rf)
    {
        if(rf)
        {
            ubo_camera_info = rf->CreateUBO<UBOCameraInfo>(&mtl::SBS_CameraInfo);
            desc_binding    = new DescriptorBinding(DescriptorSetType::Camera);
            if(desc_binding && ubo_camera_info)
                desc_binding->AddUBO(ubo_camera_info);
        }
    }
}//namespace hgl::graph
