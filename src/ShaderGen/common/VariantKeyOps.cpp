#include "VariantKeyOps.h"

#include <hgl/mtl/Material2DCreateConfig.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/RecipeToKey.h>
#include <hgl/mtl/SamplerSlot.h>

#include <cstdio>

namespace hgl::graph::mtl::routing
{

std::string FormatVariantKeyForLog(const MaterialVariantKey &key,
                                   const bool include_extended_fields)
{
    std::string text;
    text.reserve(320);

    text += "hash=";
    text += std::to_string(static_cast<unsigned long long>(key.Hash()));
    text += " ST=";
    text += std::to_string(static_cast<unsigned>(key.surface_type));
    text += " GM=";
    text += std::to_string(static_cast<unsigned>(key.geometry_mode));

    if (include_extended_fields)
    {
        text += " blend=";
        text += std::to_string(static_cast<unsigned>(key.blend_mode));
        text += " pass=";
        text += std::to_string(static_cast<unsigned>(key.pass_hint));
    }

    text += " sky=";
    text += std::to_string(static_cast<unsigned>(key.sky_ambient_model));
    text += " light=";
    text += std::to_string(static_cast<unsigned>(key.lighting_model));

    if (include_extended_fields)
    {
        text += " eff=0x";

        char hex64[24] = {};
        std::snprintf(hex64, sizeof(hex64), "%016llX",
                      static_cast<unsigned long long>(key.effective_feature_mask));
        text += hex64;
    }

    text += " tex_bits=0x";

    char hex[16] = {};
    std::snprintf(hex, sizeof(hex), "%08X", key.texture_source_bits);
    text += hex;
    text += " sampler_bits=0x";
    std::snprintf(hex, sizeof(hex), "%08X", key.sampler_feature_bits);
    text += hex;
    text += " slots=[";

    for (size_t i = 0; i < SamplerSlotCount; ++i)
    {
        if (i > 0)
            text += ",";

        const SamplerSlot slot = static_cast<SamplerSlot>(i);
        text += SamplerSlotNameList[i];
        text += ":";
        text += std::to_string(static_cast<unsigned>(key.GetTextureSourceMode(slot)));
    }

    text += "]";
    text += " va_bits=0x";
    std::snprintf(hex, sizeof(hex), "%08X", key.vertex_attribute_feature_bits);
    text += hex;
    text += " extra_bits=0x";
    std::snprintf(hex, sizeof(hex), "%08X", key.extra_feature_bits);
    text += hex;
    return text;
}

void ApplyCreateConfigOverrides(MaterialVariantKey &key,
                                const MaterialCreateConfig *cfg)
{
    if (!cfg)
        return;

    // Billboard blend mode is selected at runtime and must override routed defaults.
    if (const auto *billboard_cfg = AsBillboard(cfg))
    {
        key.blend_mode = billboard_cfg->blend_mode;
        key.pass_hint = detail::GetPrimaryPassForBlendMode(billboard_cfg->blend_mode);
    }

    if (const auto *cfg3d = As3D(cfg))
    {
        key.sky_ambient_model = cfg3d->sky_ambient_model;
        key.lighting_model = cfg3d->lighting_model;

        // Phase 2: use resolved feature intent mask as key-level truth when present.
        key.effective_feature_mask = cfg3d->effective_feature_mask;
    }

    if (cfg->override_geometry_mode)
        key.geometry_mode = cfg->geometry_mode_override;

    if (cfg->texture_source_bits_override != 0)
    {
        key.texture_source_bits = cfg->texture_source_bits_override;

        if (cfg->sampler_feature_bits_override != 0)
        {
            key.sampler_feature_bits = cfg->sampler_feature_bits_override;
        }
        else
        {
            key.sampler_feature_bits = 0;

            for (uint8_t s = 0; s < uint8_t(SamplerSlot::RANGE_SIZE); ++s)
            {
                const TextureSourceMode mode = TextureSourceMode(
                    (key.texture_source_bits >> (uint32_t(s) * MaterialVariantKey::TextureSourceBitsPerSlot))
                    & MaterialVariantKey::TextureSourceMask);

                if (mode != TextureSourceMode::None)
                    key.sampler_feature_bits |= SamplerFeatureBit(SamplerSlot(s));
            }
        }
    }
    else if (cfg->sampler_feature_bits_override != 0)
    {
        key.sampler_feature_bits = cfg->sampler_feature_bits_override;
    }
}

} // namespace hgl::graph::mtl::routing
