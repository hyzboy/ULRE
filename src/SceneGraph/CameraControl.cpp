#include<hgl/graph/camera/CameraControl.h>
#include<hgl/graph/VKDescriptorSetType.h>
#include<hgl/graph/VKDescriptorBindingManage.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/math/geometry/Ray.h>

namespace hgl::graph
{
    void CameraControl::ZoomFOV(int adjust)
    {
        if(!camera)
            return;

        constexpr float MinFOV = 10;
        constexpr float MaxFOV = 180;

        camera->fovY += float(adjust) / 10.0f;

        if(adjust < 0 && camera->fovY < MinFOV)camera->fovY = MinFOV;else
        if(adjust > 0 && camera->fovY > MaxFOV)camera->fovY = MaxFOV;
    }
}//namespace hgl::graph
