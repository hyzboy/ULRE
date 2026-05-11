#pragma once

#include <hgl/mtl/SurfaceType.h>
#include <hgl/mtl/RenderAlphaMode.h>
#include <hgl/mtl/PassType.h>
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/mtl/SkyLight.h>
#include <hgl/mtl/LightingModel.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/common/PositionProvider.h>
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
        uint64            variant_row_name_hash = 0;
        SurfaceType       surface_type        = SurfaceType::Unlit;
        GeometryMode      geometry_mode       = GeometryMode::Mesh3D;

        PositionProviderId position_provider              = PositionProviderId::DirectVec3;

        uint32            texture_source_bits           = 0;
        uint32            sampler_feature_bits          = 0;
        uint32            vertex_attribute_feature_bits = 0;
        uint32            extra_feature_bits            = 0;
        RenderAlphaMode   blend_mode          = RenderAlphaMode::Opaque;
        PassType          pass_hint           = PassType::ForwardOpaque;
        SkyLightAmbientModel sky_ambient_model = SkyLightAmbientModel::Simple;
        LightingModel lighting_model = LightingModel::Lambert;

        // Phase 2: Effective feature mask (resolved from intent_features via MaterialRecipe)
        // When non-zero, represents the authoritative feature set for this variant routing decision.
        uint64            effective_feature_mask = 0;

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

        /// Check if any texture slot uses the given SourceMode (bitmask-based check).
        bool HasAnyTextureMode(const TextureSourceMode mode) const noexcept
        {
            const uint32 mode_bits = uint32(mode) & TextureSourceMask;
            for (uint32 shift = 0; shift < 32; shift += TextureSourceBitsPerSlot)
            {
                if (((texture_source_bits >> shift) & TextureSourceMask) == mode_bits)
                    return true;
            }
            return false;
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

        uint64 Hash() const noexcept
        {
            uint64 h = hgl::hash::FNV1aInit<uint64>();

            h = hgl::hash::FNV1aAppend(h, variant_row_name_hash);
            h = hgl::hash::FNV1aAppend(h, surface_type);
            h = hgl::hash::FNV1aAppend(h, geometry_mode);
            h = hgl::hash::FNV1aAppend(h, texture_source_bits);
            h = hgl::hash::FNV1aAppend(h, sampler_feature_bits);
            h = hgl::hash::FNV1aAppend(h, vertex_attribute_feature_bits);
            h = hgl::hash::FNV1aAppend(h, extra_feature_bits);
            h = hgl::hash::FNV1aAppend(h, position_provider);
            h = hgl::hash::FNV1aAppend(h, blend_mode);
            h = hgl::hash::FNV1aAppend(h, pass_hint);
            h = hgl::hash::FNV1aAppend(h, sky_ambient_model);
            h = hgl::hash::FNV1aAppend(h, lighting_model);
            
            // Phase 3: Include effective_feature_mask in cache key computation
            if (effective_feature_mask != 0)
                h = hgl::hash::FNV1aAppend(h, effective_feature_mask);

            return h;
        }

        bool operator==(const MaterialVariantKey &rhs) const noexcept
        {
            return variant_row_name_hash == rhs.variant_row_name_hash
                && surface_type == rhs.surface_type
                && geometry_mode == rhs.geometry_mode
                && position_provider == rhs.position_provider
                && texture_source_bits == rhs.texture_source_bits
                && sampler_feature_bits == rhs.sampler_feature_bits
                && vertex_attribute_feature_bits == rhs.vertex_attribute_feature_bits
                && extra_feature_bits == rhs.extra_feature_bits
                && blend_mode == rhs.blend_mode
                && pass_hint == rhs.pass_hint
                && sky_ambient_model == rhs.sky_ambient_model
                && lighting_model == rhs.lighting_model
                && effective_feature_mask == rhs.effective_feature_mask;
        }
    };
}
