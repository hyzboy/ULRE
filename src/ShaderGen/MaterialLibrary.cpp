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

std::string GetBuiltinMaterialVariantRowSnapshot()
{
    return GetBuiltinVariantRegistry().DumpBuiltinRowSnapshot();
}

std::string GetBuiltinMaterialPresetAuditSnapshot()
{
    const auto &registry = GetBuiltinVariantRegistry();

    std::string out;
    out.reserve(8192);
    out += "# Builtin MaterialPreset Audit Snapshot\n";
    out += "preset|resolved_preset|row_name|primitive|vertex_input|vertex_policy|surface_model|surface_type|geometry_mode|blend|pass|schema|def_hint|vs_features|fs_features|resources|textures|runtime_transition|explicit_axes|legacy_inference\n";

    registry.ForEachBuiltinRow([&](const MaterialVariantRow &row)
    {
        out += GetMaterialPresetName(row.preset) ? GetMaterialPresetName(row.preset) : "<unnamed>";
        out += "|";

        const MaterialPreset resolved_preset = ResolveMaterialPresetForLOD(row.preset, GetDefaultMaterialLOD());
        out += GetMaterialPresetName(resolved_preset) ? GetMaterialPresetName(resolved_preset) : "<unnamed>";
        out += "|";

        out += row.name ? row.name : "";
        out += "|";
        out += std::to_string(static_cast<uint32>(row.primitive));
        out += "|";
        out += GetVertexInputProfileName(row.vertex_input);
        out += "|";
        out += GetVertexTransformPolicyName(row.vertex_policy);
        out += "|";
        out += GetSurfaceShadingModelName(row.surface_model);
        out += "|";
        out += std::to_string(static_cast<uint32>(row.surface_type));
        out += "|";
        out += std::to_string(static_cast<uint32>(row.geometry_mode));
        out += "|";
        out += std::to_string(static_cast<uint32>(row.blend));
        out += "|";
        out += std::to_string(static_cast<uint32>(row.pass));
        out += "|";
        out += std::to_string(static_cast<uint32>(row.schema));
        out += "|";
        out += GetStaticMaterialDefIdHintName(row.def_hint);
        out += "|";

        auto format_stage = [](const ShaderStageFeatureDesc &desc)
        {
            std::string text;
            bool first = true;
            for (size_t i = 0; i < static_cast<size_t>(VertexAttrib::RANGE_SIZE); ++i)
            {
                const auto attrib = static_cast<VertexAttrib>(i);
                if (!desc.HasVertexAttrib(attrib))
                    continue;

                if (!first)
                    text += ",";
                text += GetVertexAttribName(attrib);
                first = false;
            }

            if (desc.has_direction)
            {
                if (!first)
                    text += ",";
                text += "Direction";
                first = false;
            }

            if (desc.has_clip_pos)
            {
                if (!first)
                    text += ",";
                text += "ClipPos";
                first = false;
            }

            return first ? std::string("None") : text;
        };

        auto format_resources = [](const MaterialResourceRequirements &res)
        {
            std::string text;
            bool first = true;
            auto append = [&](const char *name, const bool enabled)
            {
                if (!enabled)
                    return;
                if (!first)
                    text += ",";
                text += name;
                first = false;
            };

            append("Viewport", res.needs_viewport);
            append("Camera", res.needs_camera);
            append("Sky", res.needs_sky);
            append("Transform", res.needs_transform);
            append("MaterialInstance", res.needs_material_instance);
            append("MaterialTextureIndex", res.needs_material_texture_index);
            append("ColorPalette", res.needs_color_palette);
            append("Lighting", res.enable_lighting);
            return first ? std::string("None") : text;
        };

        auto format_textures = [](const MaterialVariantRow &audit_row)
        {
            if (audit_row.texture_count == 0)
                return std::string("None");

            std::string text;
            for (uint32 i = 0; i < audit_row.texture_count; ++i)
            {
                if (i > 0)
                    text += ",";
                text += SamplerSlotNameList[static_cast<size_t>(audit_row.textures[i].slot)];
                text += ":";
                text += std::to_string(static_cast<uint32>(audit_row.textures[i].source_mode));
            }
            return text;
        };

        out += format_stage(row.vs_features);
        out += "|";
        out += format_stage(row.fs_features);
        out += "|";
        out += format_resources(row.resources);
        out += "|";
        out += format_textures(row);
        out += "|";

        if (row.blend == RenderAlphaMode::Dither)
            out += "DefaultDither";
        else if (row.surface_model == SurfaceShadingModel::StandardLambert
              || row.surface_model == SurfaceShadingModel::StandardBlinnPhong
              || row.surface_model == SurfaceShadingModel::StandardPBR
              || row.surface_model == SurfaceShadingModel::PBRColor)
            out += "ReservedForAutoDitherOverlay";
        else
            out += "None";

        out += "|";
        out += "preset,row,vs_features,fs_features,resources,textures,schema,def_hint";
        out += "|";

        std::string legacy;
        if (row.resources.enable_lighting)
            legacy += legacy.empty() ? "lighting_model still also mirrored in key" : ",lighting_model still also mirrored in key";
        if (row.resources.needs_sky)
            legacy += legacy.empty() ? "sky_model still also mirrored in key" : ",sky_model still also mirrored in key";
        if (row.vertex_policy == VertexTransformPolicy::BillboardCameraFacing || row.vertex_policy == VertexTransformPolicy::BillboardAxisLocked)
            legacy += legacy.empty() ? "billboard behavior still depends on dedicated row template path" : ",billboard behavior still depends on dedicated row template path";
        if (row.vertex_policy == VertexTransformPolicy::TerrainGrid)
            legacy += legacy.empty() ? "terrain behavior still depends on dedicated row template path" : ",terrain behavior still depends on dedicated row template path";
        if (row.vertex_policy == VertexTransformPolicy::Sky)
            legacy += legacy.empty() ? "sky fallback still exists in key-based path" : ",sky fallback still exists in key-based path";
        legacy += legacy.empty() ? "custom descriptors may now bind row explicitly via MaterialVariantDesc::bound_row" : ",custom descriptors may now bind row explicitly via MaterialVariantDesc::bound_row";
        out += legacy.empty() ? "None" : legacy;
        out += "\n";
    });

    return out;
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

struct PresetResolveEntry
{
    MaterialPreset preset;
    const char *name;
};

static bool IsSemanticMaterialPreset(const MaterialPreset preset)
{
    switch(preset)
    {
        case MaterialPreset::Checkerboard3D:     // alias → Standard
        case MaterialPreset::HumanSkin:
        case MaterialPreset::AmphibiansSkin:
        case MaterialPreset::Wood:
        case MaterialPreset::TreeBark:
        case MaterialPreset::Stone:
        case MaterialPreset::Leaf:
        case MaterialPreset::Metal:
        case MaterialPreset::BirdFeathers:
        case MaterialPreset::Scales:
            return true;
        default:
            return false;
    }
}

static const PresetResolveEntry kPresetResolveTable[] =
{
    // Canonical presets (one entry per kBuiltinVariants preset)
    {MaterialPreset::Checkerboard3D,       "Checkerboard3D"},      // alias → Standard via IsSemanticMaterialPreset
    {MaterialPreset::VertexColor2D,        "VertexColor2D"},
    {MaterialPreset::PureColor2D,          "PureColor2D"},
    {MaterialPreset::PureTexture2D,        "PureTexture2D"},
    {MaterialPreset::Text2D,               "Text2D"},
    {MaterialPreset::PureColor3D,          "PureColor3D"},
    {MaterialPreset::VertexColor3D,        "VertexColor3D"},
    {MaterialPreset::VertexLuminance3D,    "VertexLuminance3D"},
    {MaterialPreset::VertexLuminance2D,    "VertexLuminance2D"},
    {MaterialPreset::VertexPaletteColor3D, "VertexPaletteColor3D"},
    {MaterialPreset::Gizmo3D,              "Gizmo3D"},
    {MaterialPreset::TerrainGrid,          "TerrainGrid"},
    {MaterialPreset::SkyMinimal,           "SkyMinimal"},
    {MaterialPreset::Billboard2DDynamic,   "Billboard2DDynamic"},
    {MaterialPreset::Billboard2DFixed,     "Billboard2DFixed"},
    {MaterialPreset::Standard,             "Standard"},
    {MaterialPreset::PBRColor3D,           "PBRColor3D"},
    // Semantic aliases (LOD reserved, current lod=0 target is Standard)
    {MaterialPreset::HumanSkin,            "HumanSkin"},
    {MaterialPreset::AmphibiansSkin,       "AmphibiansSkin"},
    {MaterialPreset::Wood,                 "Wood"},
    {MaterialPreset::TreeBark,             "TreeBark"},
    {MaterialPreset::Stone,                "Stone"},
    {MaterialPreset::Leaf,                 "Leaf"},
    {MaterialPreset::Metal,                "Metal"},
    {MaterialPreset::BirdFeathers,         "BirdFeathers"},
    {MaterialPreset::Scales,               "Scales"},
    // PCG / procedural presets
    {MaterialPreset::FullscreenTriangle,   "FullscreenTriangle"},
};

static const PresetResolveEntry *FindPresetResolveEntry(const MaterialPreset preset)
{
    for(const auto &entry:kPresetResolveTable)
        if(entry.preset==preset)
            return &entry;

    return nullptr;
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
    switch(lod)
    {
        case MaterialLOD::Base:
        default:
            // Current bootstrap behavior: semantic presets still reuse the Standard family.
            if(IsSemanticMaterialPreset(preset))
                return MaterialPreset::Standard;

            return preset;
    }
}

MaterialVariantKey RouteKey(MaterialPreset preset,
                            uint32 extra_attrib_bits,
                            const RuntimeKeyOverrides &ov) noexcept
{
    // Step 1: resolve semantic alias → canonical preset via LOD table.
    const MaterialPreset resolved_preset =
        ResolveMaterialPresetForLOD(preset, GetDefaultMaterialLOD());

    // Step 2: scan kBuiltinVariants for the best matching entry.
    //   • If ov.blend_mode is set, select the entry whose blend field matches.
    //   • If ov.lighting_model is set, additionally filter on lighting field.
    //   • First match wins (table entries ordered from most common to rarest).
    const BuiltinVariantEntry *found = nullptr;
    for (size_t i = 0; i < kBuiltinVariantsCount; ++i)
    {
        const auto &e = kBuiltinVariants[i];
        if (e.preset != resolved_preset)                           continue;
        if (ov.blend_mode     && e.blend    != *ov.blend_mode)    continue;
        if (ov.lighting_model && e.lighting != *ov.lighting_model) continue;
        found = &e;
        break;
    }

    if (!found)
    {
        std::fprintf(stderr,
            "[MaterialLibrary] ERROR: RouteKey no builtin entry for preset=%u\n",
            static_cast<unsigned>(preset));
        return MaterialVariantKey{};
    }

    // Step 3: build the base key from the matched entry.
    //   blend_mode and lighting_model are already correct from entry selection.
    MaterialVariantKey key = BuildKey(*found);

    // Step 4: OR-merge caller-supplied extra vertex attribute bits.
    key.vertex_attribute_feature_bits |= extra_attrib_bits;
    key.vertex_attribute_feature_bits |= ov.extra_vertex_attrib_bits;

    // Step 5: apply remaining overrides (not covered by entry selection).
    if (ov.position_provider) key.position_provider = *ov.position_provider;  // Step 11.D
    if (ov.pass_hint)         key.pass_hint         = *ov.pass_hint;
    if (ov.sky_ambient_model) key.sky_ambient_model = *ov.sky_ambient_model;

    return key;
}

const char *GetMaterialPresetName(const MaterialPreset mtl_id)
{
    const PresetResolveEntry *entry=FindPresetResolveEntry(mtl_id);
    return entry?entry->name:nullptr;
}

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
            RegistryLookupOptions routing_opts{};
            routing_opts.preferred_factory_type = e.preset;
            const MaterialVariantDesc *found =
                GetBuiltinVariantRegistry().QueryVariantWithCanonicalFallback(k, nullptr, routing_opts);
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

        return MaterialFactory3D::Create(MaterialPreset::Checkerboard3D,
                                         profile,
                                         nullptr,
                                         MaterialVariantKey{},
                                         cfg3d);
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

    MaterialVariantKey registry_lookup_key = key;
    if (registry_lookup_key.surface_type == SurfaceType::Standard
     && registry_lookup_key.geometry_mode == GeometryMode::Mesh3D)
    {
        // Standard Mesh3D descriptor selection is not split by sky model.
        // Keep runtime-requested sky model on the original key for downstream
        // assembly/diagnostics, but use canonical sky for registry descriptor lookup.
        registry_lookup_key.sky_ambient_model = SkyLightAmbientModel::Simple;
    }

    MaterialVariantKey resolved_key{};
    RegistryLookupOptions lookup_opts{};
    if(cfg->preset_name)
    {
        for (const auto &e : kPresetResolveTable)
        {
            if (std::strcmp(cfg->preset_name, e.name) == 0)
            {
                lookup_opts.preferred_factory_type = ResolveMaterialPresetForLOD(e.preset, GetDefaultMaterialLOD());
                break;
            }
        }
    }

    const MaterialVariantDesc *variant_desc = GetBuiltinVariantRegistry().QueryVariantWithCanonicalFallback(registry_lookup_key,
                                                                                                             &resolved_key,
                                                                                                             lookup_opts);
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

MaterialCreateInfo *CreateMaterialCreateInfo(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialPreset mtl_id,
                                             MaterialCreateConfig *cfg)
{
    const MaterialLOD lod = GetDefaultMaterialLOD();
    const MaterialPreset resolved_preset = ResolveMaterialPresetForLOD(mtl_id, lod);
    const PresetResolveEntry *entry=FindPresetResolveEntry(mtl_id);
    if(entry&&resolved_preset!=mtl_id)
    {
        std::printf(
            "[MaterialLibrary] Preset alias resolved preset=%u (%s) -> canonical=%u (lod=%u)\n",
            static_cast<unsigned>(mtl_id),
            entry->name?entry->name:"<null>",
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
