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
        Simple,
        Array,
        Atlas,

        ENUM_CLASS_RANGE(None, Atlas)
    };

    // Semantic texture slots for per-slot source mode control.
    // This keeps VariantKey compact while still allowing each texture to
    // independently choose None/Simple/Array/Atlas.
    enum class TextureSlot : uint8
    {
        BaseColor = 0,
        Normal,
        Roughness,
        AO,
        Emissive,
        Detail,
        Special0,
        Special1,
        Special2,
        Special3,
        Special4,
        Special5,

        Count,
        ENUM_CLASS_RANGE(BaseColor, Special5)
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
        // Legacy coarse mode (whole-material). Kept for compatibility with
        // existing call sites while migrating to per-slot texture_source_bits.
        TextureSourceMode texture_source_mode = TextureSourceMode::None;
        // 2 bits per TextureSlot entry. 0=None, 1=Simple, 2=Array, 3=Atlas.
        uint32            texture_source_bits = 0;
        uint32            feature_bits        = VF_None;
        BlendMode         blend_mode          = BlendMode::Opaque;
        PassType          pass_hint           = PassType::ForwardOpaque;
        QualityTier       quality_tier        = QualityTier::Medium;

        static constexpr uint32 TextureSourceBitsPerSlot = 2;
        static constexpr uint32 TextureSourceSlotCount   = uint32(TextureSlot::Count);
        static constexpr uint32 TextureSourceMask        = (1u << TextureSourceBitsPerSlot) - 1u;

        void SetTextureSourceMode(const TextureSlot slot, const TextureSourceMode mode) noexcept
        {
            const uint32 shift = uint32(slot) * TextureSourceBitsPerSlot;
            texture_source_bits &= ~(TextureSourceMask << shift);
            texture_source_bits |= (uint32(mode) & TextureSourceMask) << shift;
        }

        TextureSourceMode GetTextureSourceMode(const TextureSlot slot) const noexcept
        {
            const uint32 shift = uint32(slot) * TextureSourceBitsPerSlot;
            return TextureSourceMode((texture_source_bits >> shift) & TextureSourceMask);
        }

        bool HasAnyTextureSourceBits() const noexcept
        {
            return texture_source_bits != 0;
        }

        TextureSourceMode GetPrimaryTextureSourceMode() const noexcept
        {
            // Prefer per-slot BaseColor when available; fall back to legacy field.
            return HasAnyTextureSourceBits()
                ? GetTextureSourceMode(TextureSlot::BaseColor)
                : texture_source_mode;
        }

        uint64 Hash() const noexcept
        {
            uint64 h = 1469598103934665603ull;
            auto mix = [&h](const uint64 v)
            {
                h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
            };

            mix(static_cast<uint64>(surface_type));
            mix(static_cast<uint64>(geometry_mode));
            mix(static_cast<uint64>(texture_source_mode));
            mix(static_cast<uint64>(texture_source_bits));
            mix(static_cast<uint64>(feature_bits));
            mix(static_cast<uint64>(blend_mode));
            mix(static_cast<uint64>(pass_hint));
            mix(static_cast<uint64>(quality_tier));

            return h;
        }

        bool operator==(const MaterialVariantKey &rhs) const noexcept
        {
            return surface_type == rhs.surface_type
                && geometry_mode == rhs.geometry_mode
                && texture_source_mode == rhs.texture_source_mode
                && texture_source_bits == rhs.texture_source_bits
                && feature_bits == rhs.feature_bits
                && blend_mode == rhs.blend_mode
                && pass_hint == rhs.pass_hint
                && quality_tier == rhs.quality_tier;
        }
    };
}
