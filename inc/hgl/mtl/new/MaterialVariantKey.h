#pragma once

#include <hgl/mtl/new/SurfaceType.h>
#include <hgl/mtl/new/BlendMode.h>
#include <hgl/mtl/new/PassType.h>
#include <hgl/mtl/new/QualityTier.h>

namespace hgl::graph::mtl
{
    enum class GeometryMode : uint8
    {
        Mesh3D = 0,
        Quad2D,
        ScreenRect,
        BillboardCameraFacing,
        BillboardAxisLocked,

        ENUM_CLASS_RANGE(Mesh3D, BillboardAxisLocked)
    };

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
        VF_UsePos2D         = 1u << 6,
    };

    struct MaterialVariantKey
    {
        SurfaceType       surface_type        = SurfaceType::Unlit;
        GeometryMode      geometry_mode       = GeometryMode::Mesh3D;
        TextureSourceMode texture_source_mode = TextureSourceMode::None;
        uint32            feature_bits        = VF_None;
        BlendMode         blend_mode          = BlendMode::Opaque;
        PassType          pass_hint           = PassType::ForwardOpaque;
        QualityTier       quality_tier        = QualityTier::Medium;

        uint64 Hash() const noexcept
        {
            uint64 h = 0;
            h ^= static_cast<uint64>(surface_type)        << 0;
            h ^= static_cast<uint64>(geometry_mode)       << 8;
            h ^= static_cast<uint64>(texture_source_mode) << 16;
            h ^= static_cast<uint64>(feature_bits)        << 24;
            h ^= static_cast<uint64>(blend_mode)          << 56;
            h ^= static_cast<uint64>(pass_hint)           << 57;
            return h;
        }

        bool operator==(const MaterialVariantKey &rhs) const noexcept
        {
            return surface_type == rhs.surface_type
                && geometry_mode == rhs.geometry_mode
                && texture_source_mode == rhs.texture_source_mode
                && feature_bits == rhs.feature_bits
                && blend_mode == rhs.blend_mode
                && pass_hint == rhs.pass_hint
                && quality_tier == rhs.quality_tier;
        }
    };
}
