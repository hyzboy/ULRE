#pragma once

#include <hgl/mtl/new/SurfaceType.h>
#include <hgl/mtl/new/BlendMode.h>
#include <hgl/mtl/new/PassType.h>
#include <hgl/mtl/new/QualityTier.h>

namespace hgl::graph::mtl
{
    enum class TextureSourceMode : uint8
    {
        None = 0,
        Tex2D,
        Tex2DArray,
        Atlas,

        ENUM_CLASS_RANGE(None, Atlas)
    };

    enum VariantFeatureBits : uint32
    {
        VF_None             = 0,
        VF_HasBaseColorTex  = 1u << 0,
        VF_HasNormalTex     = 1u << 1,
        VF_HasRoughnessTex  = 1u << 2,
        VF_UseVertexColor   = 1u << 3,
        VF_UseVertexLum     = 1u << 4,
        VF_DebugShading     = 1u << 5,
    };

    struct MaterialVariantKey
    {
        SurfaceType       surface_type        = SurfaceType::Unlit;
        TextureSourceMode texture_source_mode = TextureSourceMode::None;
        uint32            feature_bits        = VF_None;
        BlendMode         blend_mode          = BlendMode::Opaque;
        PassType          pass_hint           = PassType::ForwardOpaque;
        QualityTier       quality_tier        = QualityTier::Medium;

        bool operator==(const MaterialVariantKey &rhs) const noexcept
        {
            return surface_type == rhs.surface_type
                && texture_source_mode == rhs.texture_source_mode
                && feature_bits == rhs.feature_bits
                && blend_mode == rhs.blend_mode
                && pass_hint == rhs.pass_hint
                && quality_tier == rhs.quality_tier;
        }
    };
}
