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

// ---------------------------------------------------------------------------
// [Step 3.5 T1] Variant key helpers below are DEPRECATED.
// All factory call sites must migrate to hgl::graph::mtl::RouteKey() before
// Step 4 can begin (see VertexInputFormat_plan.md, T2 row in the migration
// table). MakeBillboardKeyBase() remains while billboard factories are
// migrated; it will be subsumed into RouteKey()'s preset-default path in T3.
// ---------------------------------------------------------------------------

[[deprecated("[Step 3.5 T1] use hgl::graph::mtl::RouteKey(preset)")]]
inline MaterialVariantKey MakeVariantKey()
{
    return MaterialVariantKey{};
}

[[deprecated("[Step 3.5 T1] use hgl::graph::mtl::RouteKey(preset)")]]
inline MaterialVariantKey MakeVariantKeyWithSurface(const SurfaceType surface_type)
{
    MaterialVariantKey key{};
    key.surface_type = surface_type;
    return key;
}

[[deprecated("[Step 3.5 T1] use RouteKey(preset, VertexAttribFeatureBit(attrib), {})")]]
inline MaterialVariantKey MakeVariantKeyWithAttrib(const VertexAttrib attrib)
{
    MaterialVariantKey key{};
    key.SetVertexAttribEnabled(attrib);
    return key;
}

inline Material3DCreateConfig MakeLocalConfig(const Material3DCreateConfig *cfg)
{
    return cfg ? *cfg : Material3DCreateConfig();
}

inline PassType BlendModeToPassHint(const RenderAlphaMode blend_mode)
{
    switch (blend_mode) {
    case RenderAlphaMode::Masked:          return PassType::ForwardMasked;
    case RenderAlphaMode::Dither:          return PassType::ForwardDither;
    case RenderAlphaMode::Opaque:          return PassType::ForwardOpaque;
    case RenderAlphaMode::AlphaToCoverage: return PassType::ForwardA2C;
    default:                               return PassType::ForwardTransparent;
    }
}

[[deprecated("use RouteKey(preset, 0, RuntimeKeyOverrides{.blend_mode=...}) -- Step 3.5 T3")]]
inline MaterialVariantKey MakeBillboardKeyBase(const RenderAlphaMode blend_mode)
{
    MaterialVariantKey key;
    key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Simple);
    key.blend_mode = blend_mode;
    key.pass_hint = BlendModeToPassHint(blend_mode);
    return key;
}

}
}
}
}
