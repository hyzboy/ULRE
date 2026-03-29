#pragma once

#include<hgl/mtl/FixedMaterialDef.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/new/MaterialVariantKey.h>

namespace hgl
{
namespace graph
{
namespace mtl
{
namespace build3d
{

inline FixedUBODescriptors MakeViewportCameraUBOs()
{
    return {
        UBODescriptorSemantic::ViewportInfo,
        UBODescriptorSemantic::CameraInfo,
    };
}

inline FixedUBODescriptors MakeViewportCameraSkyUBOs()
{
    return {
        UBODescriptorSemantic::ViewportInfo,
        UBODescriptorSemantic::CameraInfo,
        UBODescriptorSemantic::SkyInfo,
    };
}

inline FixedSSBODescriptors MakeTransformSSBOs(const bool with_material_instance)
{
    FixedSSBODescriptors descriptors = {
        SSBODescriptorSemantic::TransformData,
        SSBODescriptorSemantic::TransformID,
    };

    if(with_material_instance)
    {
        descriptors.insert(SSBODescriptorSemantic::MaterialInstanceID);
        descriptors.insert(SSBODescriptorSemantic::MaterialInstanceData);
    }

    return descriptors;
}

inline MaterialVariantKey MakeVariantKey()
{
    return MaterialVariantKey{};
}

inline MaterialVariantKey MakeVariantKeyWithSurface(const SurfaceType surface_type)
{
    MaterialVariantKey key{};
    key.surface_type = surface_type;
    return key;
}

inline MaterialVariantKey MakeVariantKeyWithAttrib(const VertexAttrib attrib)
{
    MaterialVariantKey key{};
    key.SetVertexAttribEnabled(attrib);
    return key;
}

inline MaterialVariantKey MakeVariantKeyWithAttribAndDebug(const VertexAttrib attrib)
{
    MaterialVariantKey key{};
    key.SetVertexAttribEnabled(attrib);
    key.SetDebugShading(true);
    return key;
}

inline Material3DCreateConfig MakeLocalConfig(const Material3DCreateConfig *cfg)
{
    return cfg ? *cfg : Material3DCreateConfig();
}

}
}
}
}
