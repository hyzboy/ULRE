#pragma once

#include <hgl/mtl/new/SurfaceType.h>
#include <hgl/mtl/new/BlendMode.h>
#include <hgl/mtl/new/PassType.h>
#include <hgl/mtl/SamplerName.h>
#include <hgl/common/VertexAttribDef.h>

namespace hgl::graph::mtl
{
    using SamplerSlot = SamplerSlot;
    using TextureSourceMode = TextureSourceMode;

    constexpr uint32 SamplerFeatureBit(const SamplerSlot slot) noexcept
    {
        return 1u << static_cast<uint32>(slot);
    }

    constexpr uint32 VertexAttribFeatureBit(const VertexAttrib attrib) noexcept
    {
        return 1u << static_cast<uint32>(attrib);
    }

    enum ExtraFeatureBits : uint32
    {
        EF_None          = 0,
        EF_DebugShading  = 1u << 0,
    };

    enum class GeometryMode : uint8
    {
        Mesh3D = 0,
        Quad2D,
        ScreenRect,
        BillboardCameraFacing,
        BillboardAxisLocked,

        ENUM_CLASS_RANGE(Mesh3D, BillboardAxisLocked)
    };

    struct MaterialVariantKey
    {
        SurfaceType       surface_type        = SurfaceType::Unlit;
        GeometryMode      geometry_mode       = GeometryMode::Mesh3D;
        // Legacy coarse mode (whole-material). Kept for compatibility with
        // existing call sites while migrating to per-slot texture_source_bits.
        TextureSourceMode texture_source_mode = TextureSourceMode::None;
        // 2 bits per SamplerSlot entry. 0=None, 1=Simple, 2=Array, 3=Atlas.
        uint32            texture_source_bits           = 0;
        uint32            sampler_feature_bits          = 0;
        uint32            vertex_attribute_feature_bits = 0;
        uint32            extra_feature_bits            = EF_None;
        BlendMode         blend_mode          = BlendMode::Opaque;
        PassType          pass_hint           = PassType::ForwardOpaque;

        static constexpr uint32 TextureSourceBitsPerSlot = 2;
        static constexpr uint32 TextureSourceSlotCount   = uint32(SamplerSlot::Count);
        static constexpr uint32 TextureSourceMask        = (1u << TextureSourceBitsPerSlot) - 1u;

        void SetTextureSourceMode(const SamplerSlot slot, const TextureSourceMode mode) noexcept
        {
            const uint32 shift = uint32(slot) * TextureSourceBitsPerSlot;
            texture_source_bits &= ~(TextureSourceMask << shift);
            texture_source_bits |= (uint32(mode) & TextureSourceMask) << shift;

            const uint32 bit = SamplerFeatureBit(slot);
            if (mode == TextureSourceMode::None)
                sampler_feature_bits &= ~bit;
            else
                sampler_feature_bits |= bit;
        }

        TextureSourceMode GetTextureSourceMode(const SamplerSlot slot) const noexcept
        {
            const uint32 shift = uint32(slot) * TextureSourceBitsPerSlot;
            return TextureSourceMode((texture_source_bits >> shift) & TextureSourceMask);
        }

        void SetHasTexture(const SamplerSlot slot, const bool enabled = true) noexcept
        {
            const uint32 bit = SamplerFeatureBit(slot);
            if (enabled)
                sampler_feature_bits |= bit;
            else
                sampler_feature_bits &= ~bit;
        }

        bool HasTexture(const SamplerSlot slot) const noexcept
        {
            return (sampler_feature_bits & SamplerFeatureBit(slot)) != 0;
        }

        void SetVertexAttribEnabled(const VertexAttrib attrib, const bool enabled = true) noexcept
        {
            const uint32 bit = VertexAttribFeatureBit(attrib);
            if (enabled)
                vertex_attribute_feature_bits |= bit;
            else
                vertex_attribute_feature_bits &= ~bit;
        }

        bool HasVertexAttrib(const VertexAttrib attrib) const noexcept
        {
            return (vertex_attribute_feature_bits & VertexAttribFeatureBit(attrib)) != 0;
        }

        void SetDebugShading(const bool enabled = true) noexcept
        {
            if (enabled)
                extra_feature_bits |= EF_DebugShading;
            else
                extra_feature_bits &= ~EF_DebugShading;
        }

        bool IsDebugShading() const noexcept
        {
            return (extra_feature_bits & EF_DebugShading) != 0;
        }

        bool HasAnyTextureSourceBits() const noexcept
        {
            return texture_source_bits != 0;
        }

        TextureSourceMode GetPrimaryTextureSourceMode() const noexcept
        {
            // Prefer per-slot BaseColor when available; fall back to legacy field.
            return HasAnyTextureSourceBits()
                ? GetTextureSourceMode(SamplerSlot::BaseColor)
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
            mix(static_cast<uint64>(sampler_feature_bits));
            mix(static_cast<uint64>(vertex_attribute_feature_bits));
            mix(static_cast<uint64>(extra_feature_bits));
            mix(static_cast<uint64>(blend_mode));
            mix(static_cast<uint64>(pass_hint));

            return h;
        }

        bool operator==(const MaterialVariantKey &rhs) const noexcept
        {
            return surface_type == rhs.surface_type
                && geometry_mode == rhs.geometry_mode
                && texture_source_mode == rhs.texture_source_mode
                && texture_source_bits == rhs.texture_source_bits
                && sampler_feature_bits == rhs.sampler_feature_bits
                && vertex_attribute_feature_bits == rhs.vertex_attribute_feature_bits
                && extra_feature_bits == rhs.extra_feature_bits
                && blend_mode == rhs.blend_mode
                && pass_hint == rhs.pass_hint;
        }
    };
}
