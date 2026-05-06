#pragma once

#include<hgl/mtl/StaticMaterialDef.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialVariantKey.h>

namespace hgl
{
namespace graph
{
namespace mtl
{
namespace build3d
{

inline UBOSemanticSet MakeViewportCameraUBOs()
{
    return {
        UBODescriptorSemantic::ViewportInfo,
        UBODescriptorSemantic::CameraInfo,
    };
}

inline UBOSemanticSet MakeViewportCameraSkyUBOs()
{
    return {
        UBODescriptorSemantic::ViewportInfo,
        UBODescriptorSemantic::CameraInfo,
        UBODescriptorSemantic::SkyInfo,
    };
}

inline SSBOSemanticSet MakeTransformSSBOs(const bool with_material_instance)
{
    SSBOSemanticSet descriptors = {
        SSBODescriptorSemantic::TransformData,
        SSBODescriptorSemantic::TransformID,
    };

    if(with_material_instance)
    {
        descriptors.insert(SSBODescriptorSemantic::MaterialBindingInstanceID);
        descriptors.insert(SSBODescriptorSemantic::MaterialBindingInstanceData);
    }

    return descriptors;
}

inline Material3DCreateConfig MakeLocalConfig(const Material3DCreateConfig *cfg)
{
    return cfg ? *cfg : Material3DCreateConfig();
}

}
}
}
}
