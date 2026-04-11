#pragma once

#include <hgl/mtl/SurfaceType.h>
#include <hgl/mtl/RenderAlphaMode.h>
#include <hgl/mtl/PassType.h>
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/mtl/SkyLight.h>
#include <hgl/mtl/LightingModel.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/type/FNV1a.h>
#include <cstdio>

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
        GeometryMode      geometry_mode       = GeometryMode::Mesh3D;

        [[deprecated("Do not write surface_type directly. Use SetSurfaceType / preset-driven mapping APIs.")]]
        SurfaceType       surface_type        = SurfaceType::Unlit;

        uint32            texture_source_bits           = 0;
        uint32            sampler_feature_bits          = 0;
        uint32            vertex_attribute_feature_bits = 0;
        uint32            extra_feature_bits            = static_cast<uint32>(ExtraFeature::None);
        RenderAlphaMode         blend_mode          = RenderAlphaMode::Opaque;
        PassType          pass_hint           = PassType::ForwardOpaque;
        SkyLightAmbientModel sky_ambient_model = SkyLightAmbientModel::Simple;
        LightingModel lighting_model = LightingModel::Lambert;

        static constexpr uint32 TextureSourceBitsPerSlot = 2;
        static constexpr uint32 TextureSourceSlotCount   = uint32(SamplerSlot::RANGE_SIZE);
        static constexpr uint32 TextureSourceMask        = (1u << TextureSourceBitsPerSlot) - 1u;

        SurfaceType GetSurfaceType() const noexcept
        {
            return surface_type;
        }

        // Compatibility setter during migration. New code should route through
        // MapPresetToVariantKey/MapPresetToSurfaceType instead of manually mutating surface.
        void SetSurfaceType(const SurfaceType surface) noexcept
        {
#if defined(_DEBUG)
            static bool warned = false;
            if (!warned)
            {
                warned = true;
                std::fprintf(stderr,
                    "[MaterialVariantKey] SetSurfaceType compatibility path used. "
                    "Prefer preset-driven mapping (MapPresetToVariantKey/MapPresetToSurfaceType).\n");
            }
#endif
            surface_type = surface;
        }

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
            h = hgl::hash::FNV1aAppend(h, sky_ambient_model);
            h = hgl::hash::FNV1aAppend(h, lighting_model);

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
                && pass_hint == rhs.pass_hint
                && sky_ambient_model == rhs.sky_ambient_model
                && lighting_model == rhs.lighting_model;
        }
    };
}
