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

inline PassType BlendModeToPassHint(const BlendMode blend_mode)
{
    switch (blend_mode) {
    case BlendMode::Masked:  return PassType::ForwardMasked;
    case BlendMode::Dither:  return PassType::ForwardDither;
    case BlendMode::Opaque:  return PassType::ForwardOpaque;
    default:                 return PassType::ForwardTransparent;
    }
}

inline MaterialVariantKey MakeBillboardKeyBase(const BlendMode blend_mode)
{
    MaterialVariantKey key;
    key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Simple);
    key.SetHasTexture(SamplerSlot::BaseColor);
    key.blend_mode = blend_mode;
    key.pass_hint = BlendModeToPassHint(blend_mode);
    return key;
}

}
}
}
}
