#pragma once

#include <hgl/mtl/SurfaceType.h>
#include <hgl/mtl/BlendMode.h>
#include <hgl/mtl/PassType.h>
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/type/FNV1a.h>

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

    enum class ExtraFeature : uint32
    {
        None         = 0,
        DebugShading = 1u << 0,

        ENUM_CLASS_RANGE(None, DebugShading)
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

        uint32            texture_source_bits           = 0;
        uint32            sampler_feature_bits          = 0;
        uint32            vertex_attribute_feature_bits = 0;
        uint32            extra_feature_bits            = static_cast<uint32>(ExtraFeature::None);
        BlendMode         blend_mode          = BlendMode::Opaque;
        PassType          pass_hint           = PassType::ForwardOpaque;

        static constexpr uint32 TextureSourceBitsPerSlot = 2;
        static constexpr uint32 TextureSourceSlotCount   = uint32(SamplerSlot::RANGE_SIZE);
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
            constexpr uint32 debug_shading_bit = static_cast<uint32>(ExtraFeature::DebugShading);
            if (enabled)
                extra_feature_bits |= debug_shading_bit;
            else
                extra_feature_bits &= ~debug_shading_bit;
        }

        bool IsDebugShading() const noexcept
        {
            return (extra_feature_bits & static_cast<uint32>(ExtraFeature::DebugShading)) != 0;
        }

        uint64 Hash() const noexcept
        {
            uint64 h = hgl::hash::FNV1aInit<uint64>();

            h = hgl::hash::FNV1aAppend(h, surface_type);
            h = hgl::hash::FNV1aAppend(h, geometry_mode);
            h = hgl::hash::FNV1aAppend(h, texture_source_bits);
            h = hgl::hash::FNV1aAppend(h, sampler_feature_bits);
            h = hgl::hash::FNV1aAppend(h, vertex_attribute_feature_bits);
            h = hgl::hash::FNV1aAppend(h, extra_feature_bits);
            h = hgl::hash::FNV1aAppend(h, blend_mode);
            h = hgl::hash::FNV1aAppend(h, pass_hint);

            return h;
        }

        bool operator==(const MaterialVariantKey &rhs) const noexcept
        {
            return surface_type == rhs.surface_type
                && geometry_mode == rhs.geometry_mode
                && texture_source_bits == rhs.texture_source_bits
                && sampler_feature_bits == rhs.sampler_feature_bits
                && vertex_attribute_feature_bits == rhs.vertex_attribute_feature_bits
                && extra_feature_bits == rhs.extra_feature_bits
                && blend_mode == rhs.blend_mode
                && pass_hint == rhs.pass_hint;
        }
    };
}
