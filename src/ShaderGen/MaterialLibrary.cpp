#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialVariantRegistry.h>
#include<hgl/mtl/RecipeToKey.h>
#include<hgl/shadergen/ShaderLibraryPath.h>
#include<hgl/shadergen/device/DeviceProfile.h>
#include<hgl/shadergen/MaterialFactory3D.h>
#include<cstdio>
#include "common/VariantLookupService.h"
#include "common/VariantRoutingPolicy.h"
#include "BuiltinVariantEntry.h"

namespace hgl::graph::mtl{

bool ValidateBuiltinMaterialVariants(const std::string &shader_library_path,
                                     std::vector<std::string> &diagnostics)
{
    return GetBuiltinVariantRegistry().ValidateBuiltinVariantTemplates(shader_library_path,diagnostics);
}

std::string GetBuiltinMaterialVariantSnapshot()
{
    return GetBuiltinVariantRegistry().DumpSnapshot();
}

namespace {

static std::string FormatVariantKeyForLog(const MaterialVariantKey &key)
{
    std::string text;
    text.reserve(320);

    text += "hash=";
    text += std::to_string(static_cast<unsigned long long>(key.Hash()));
    text += " ST=";
    text += std::to_string(static_cast<unsigned>(key.surface_type));
    text += " GM=";
    text += std::to_string(static_cast<unsigned>(key.geometry_mode));
    text += " sky=";
    text += std::to_string(static_cast<unsigned>(key.sky_ambient_model));
    text += " light=";
    text += std::to_string(static_cast<unsigned>(key.lighting_model));
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

}

MaterialLOD GetDefaultMaterialLOD()
{
    // Temporary bootstrap fallback: current runtime only exposes one built-in material
    // implementation level. Future forward / VBuffer paths may choose LOD from richer context
    // instead of using a single global default.
    return MaterialLOD::Base;
}

MaterialPreset ResolveMaterialPresetForLOD(const MaterialPreset preset,
                                           const MaterialLOD lod)
{
    return routing::ResolvePresetForLOD(preset,lod);
}

MaterialVariantKey MapPresetToVariantKey(const MaterialPreset mtl_id)
{
    // [Step 3.5 T1] Deprecated forwarder. RouteKey is the single source of truth
    // for variant key construction; this remains only as a thin shim until all
    // call sites in the factory layer (T2) and external callers are migrated.
    return RouteKey(mtl_id, 0u, RuntimeKeyOverrides{});
}

MaterialVariantKey RouteKey(MaterialPreset preset,
                            uint32 extra_attrib_bits,
                            const RuntimeKeyOverrides &ov) noexcept
{
    return routing::BuildRouteKey(preset,extra_attrib_bits,ov);
}

const char *GetMaterialPresetName(const MaterialPreset mtl_id)
{
    return routing::GetPresetName(mtl_id);
}

MaterialCreateInfo *CreateCheckerboard3D(const contract::PhysicalDeviceProfileLite *profile,
                                         Material3DCreateConfig *cfg);



MaterialCreateInfo *CreateMaterialCreateInfo(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialVariantKey &key,
                                             MaterialCreateConfig *cfg)
{
    static const bool s_startup_variant_validation_done = []()
    {
        std::vector<std::string> diagnostics;

        const bool ok = ValidateBuiltinMaterialVariants(GetShaderLibraryPath(),diagnostics);
        if(ok)
        {
            std::printf("[MaterialLibrary] Startup variant validation passed.\n");
            return true;
        }

        std::fprintf(stderr,
                     "[MaterialLibrary] Startup variant validation failed: %zu issue(s).\n",
                     diagnostics.size());

        for(const auto &msg:diagnostics)
            std::fprintf(stderr,"[MaterialLibrary] %s\n",msg.c_str());

        return false;
    }();

    (void)s_startup_variant_validation_done;

    // [Step 3.5 T4] Routing self-test: every kBuiltinVariants entry must round-trip
    // through the registry (BuildKey → QueryVariantWithCanonicalFallback → factory_type
    // must equal entry.preset). Any mismatch is a programming error → abort immediately.
    static const bool s_routing_consistency_ok = []() noexcept
    {
        bool all_ok = true;
        for (size_t i = 0; i < kBuiltinVariantsCount; ++i)
        {
            const auto &e = kBuiltinVariants[i];
            const MaterialVariantKey k = BuildKey(e);
            const MaterialVariantDesc *found =
                GetBuiltinVariantRegistry().QueryVariantWithCanonicalFallback(k, nullptr);
            const bool entry_ok = found
                                && found->factory_type.has_value()
                                && *found->factory_type == e.preset;
            if (!entry_ok)
            {
                std::fprintf(stderr,
                    "[MaterialLibrary] FATAL: routing self-test FAILED for entry[%zu] \"%s\""
                    " (preset=%u): registry returned %s\n",
                    i,
                    e.name,
                    static_cast<unsigned>(e.preset),
                    found ? (found->factory_type.has_value()
                             ? "wrong factory_type"
                             : "desc with no factory_type")
                          : "nullptr");
                all_ok = false;
            }
        }
        if (!all_ok)
        {
            std::fprintf(stderr,
                "[MaterialLibrary] FATAL: BuiltinVariantEntry routing self-test failed"
                " — aborting to prevent undefined behaviour in main loop.\n");
            std::abort();
        }
        std::printf("[MaterialLibrary] BuiltinVariantEntry routing self-test passed"
                    " (%zu entries).\n", kBuiltinVariantsCount);
        return true;
    }();
    (void)s_routing_consistency_ok;

    if(!cfg)
    {
        std::fprintf(stderr, "[MaterialLibrary] CreateMaterialCreateInfo failed: cfg is null\n");
        return nullptr;
    }

    if(cfg->preset_name
    && std::strcmp(cfg->preset_name, GetMaterialPresetName(MaterialPreset::Checkerboard3D)) == 0)
    {
        Material3DCreateConfig *cfg3d = As3D(cfg);
        if(!cfg3d)
        {
            std::fprintf(stderr,
                "[MaterialLibrary] CreateMaterialCreateInfo failed: Checkerboard3D requires Material3DCreateConfig\n");
            return nullptr;
        }

        return CreateCheckerboard3D(profile, cfg3d);
    }

    if(!profile)
    {
        std::fprintf(stderr,
            "[MaterialLibrary] CreateMaterialCreateInfo warning: profile is null (key_hash=%llu surface=%u geom=%u tex_mode=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X)\n",
            static_cast<unsigned long long>(key.Hash()),
            static_cast<unsigned>(key.surface_type),
            static_cast<unsigned>(key.geometry_mode),
            static_cast<unsigned>(key.GetTextureSourceMode(SamplerSlot::BaseColor)),
            key.texture_source_bits,
            key.sampler_feature_bits,
            key.vertex_attribute_feature_bits,
            key.extra_feature_bits);
    }

    const MaterialVariantKey registry_lookup_key = routing::CanonicalizeRegistryLookupKey(key, RegistryLookupOptions{});

    MaterialVariantKey resolved_key{};
    const MaterialVariantDesc *variant_desc = GetBuiltinVariantRegistry().QueryVariantWithCanonicalFallback(registry_lookup_key,&resolved_key);
    if(!variant_desc)
    {
        std::fprintf(stderr,
            "[MaterialLibrary] CreateMaterialCreateInfo failed: no registered variant (key_hash=%llu surface=%u geom=%u tex_mode=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X)\n",
            static_cast<unsigned long long>(key.Hash()),
            static_cast<unsigned>(key.surface_type),
            static_cast<unsigned>(key.geometry_mode),
            static_cast<unsigned>(key.GetTextureSourceMode(SamplerSlot::BaseColor)),
            key.texture_source_bits,
            key.sampler_feature_bits,
            key.vertex_attribute_feature_bits,
            key.extra_feature_bits);
        return nullptr;
    }

    std::fprintf(stderr,
        "[MaterialLibrary] resolved variant=%s request={%s} lookup={%s} resolved={%s}\n",
        variant_desc->variant_name.c_str(),
        FormatVariantKeyForLog(key).c_str(),
        FormatVariantKeyForLog(registry_lookup_key).c_str(),
        FormatVariantKeyForLog(resolved_key).c_str());

    if(!variant_desc->factory_type)
    {
        std::fprintf(stderr,
            "[MaterialLibrary] CreateMaterialCreateInfo failed: variant has no factory_type assigned (variant=%s key_hash=%llu)\n",
            variant_desc->variant_name.c_str(),
            static_cast<unsigned long long>(key.Hash()));
        return nullptr;
    }

    const MaterialPreset factory_type = *variant_desc->factory_type;

    if(MaterialCreateInfo *mci=MaterialFactory3D::Create(factory_type,profile,variant_desc,key,cfg))
        return mci;

    std::fprintf(stderr,
        "[MaterialLibrary] CreateMaterialCreateInfo failed: factory dispatch failed (variant=%s factory_type=%u key_hash=%llu resolved_key_hash=%llu)\n",
        variant_desc->variant_name.c_str(),
        static_cast<unsigned>(factory_type),
        static_cast<unsigned long long>(key.Hash()),
        static_cast<unsigned long long>(resolved_key.Hash()));
    return nullptr;
}

void ApplyCreateConfigToVariantKey(MaterialVariantKey &key, const MaterialCreateConfig *cfg)
{
    if (!cfg)
        return;

    // Billboard: blend_mode is per-instance and was not part of the RouteKey call
    // (RouteKey picks the first matching entry, which defaults to Opaque).
    // Apply the caller-supplied blend_mode here so the correct variant is selected.
    if (const auto *billboard_cfg = AsBillboard(cfg))
    {
        key.blend_mode = billboard_cfg->blend_mode;
        key.pass_hint  = detail::GetPrimaryPassForBlendMode(billboard_cfg->blend_mode);
    }

    if (const auto *cfg3d = As3D(cfg))
    {
        key.sky_ambient_model = cfg3d->sky_ambient_model;
        key.lighting_model = cfg3d->lighting_model;
        
        // Phase 2: Copy effective feature mask (resolved from intent_features)
        // If non-zero, this is the authoritative feature decision.
        key.effective_feature_mask = cfg3d->effective_feature_mask;
    }

    if (cfg->override_geometry_mode)
        key.geometry_mode = cfg->geometry_mode_override;

    if (cfg->texture_source_bits_override != 0)
    {
        key.texture_source_bits = cfg->texture_source_bits_override;

        // Derive primary texture source mode from per-slot bits.
        // If caller did not provide an explicit sampler feature override,
        // derive mask from per-slot texture source bits to keep key coherent.
        if (cfg->sampler_feature_bits_override != 0)
            key.sampler_feature_bits = cfg->sampler_feature_bits_override;
        else
        {
            key.sampler_feature_bits = 0;
            for (uint8_t s = 0; s < uint8_t(SamplerSlot::RANGE_SIZE); ++s)
            {
                const TextureSourceMode mode = TextureSourceMode((key.texture_source_bits >> (uint32_t(s) * MaterialVariantKey::TextureSourceBitsPerSlot))
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

MaterialCreateInfo *CreateMaterialCreateInfo(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialPreset mtl_id,
                                             MaterialCreateConfig *cfg)
{
    const MaterialLOD lod = GetDefaultMaterialLOD();
    const MaterialPreset resolved_preset = ResolveMaterialPresetForLOD(mtl_id,lod);
    const char *preset_name = GetMaterialPresetName(mtl_id);
    if(preset_name&&resolved_preset!=mtl_id)
    {
        std::printf(
            "[MaterialLibrary] Preset alias resolved preset=%u (%s) -> canonical=%u (lod=%u)\n",
            static_cast<unsigned>(mtl_id),
            preset_name,
            static_cast<unsigned>(resolved_preset),
            static_cast<unsigned>(lod));
    }

    // [Step 3.5 T1] Route through RouteKey() so this internal call site does not
    // emit a [[deprecated]] warning and remains aligned with the single-track entry.
    MaterialVariantKey key = RouteKey(resolved_preset);

    ApplyCreateConfigToVariantKey(key, cfg);

    if (resolved_preset == MaterialPreset::PBRColor3D)
    {
        key.lighting_model = LightingModel::PBR;

        if (auto *cfg3d = As3D(cfg))
            cfg3d->lighting_model = LightingModel::PBR;
    }

    std::fprintf(stderr,
        "[MaterialLibrary] request preset=%u resolved_preset=%u key={%s}\n",
        static_cast<unsigned>(mtl_id),
        static_cast<unsigned>(resolved_preset),
        FormatVariantKeyForLog(key).c_str());

    return CreateMaterialCreateInfo(profile, key, cfg);
}

}//namespace hgl::graph::mtl
