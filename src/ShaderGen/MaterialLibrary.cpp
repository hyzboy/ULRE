#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialVariantRegistry.h>
#include<hgl/mtl/MaterialPrunePolicy.h>
#include<hgl/mtl/RecipeToKey.h>
#include<hgl/shadergen/ShaderLibraryPath.h>
#include<hgl/shadergen/device/DeviceProfile.h>
#include<hgl/shadergen/MaterialFactory3D.h>
#include<hgl/shadergen/RegistryQuery.h>
#include<hgl/shadergen/registry/ErrorCodeRegistry.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/shadergen/ShaderRequirementSet.h>
#include<algorithm>
#include<atomic>
#include<cstring>
#include<cstdio>

namespace hgl::graph::mtl{

namespace {

static bool StageFeaturesDeclareAnyVertexAttrib(const ShaderStageFeatureDesc &features) noexcept
{
    for (size_t i = 0; i < static_cast<size_t>(VertexAttrib::RANGE_SIZE); ++i)
    {
        if (features.HasVertexAttrib(static_cast<VertexAttrib>(i)))
            return true;
    }

    return false;
}

static bool RowDeclaresAnyVertexAttrib(const MaterialVariantRow &row) noexcept
{
    return StageFeaturesDeclareAnyVertexAttrib(row.vs_features)
        || StageFeaturesDeclareAnyVertexAttrib(row.fs_features);
}

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

    // Build a reusable assembler for SFM inference (Phase 6 audit columns).
    const hgl::graph::CompositorAssembler sfm_assembler(GetShaderLibraryPath());

    std::string out;
    out.reserve(8192);
    out += "# Builtin MaterialPreset Audit Snapshot\n";
    out += "preset|resolved_preset|row_name|primitive|vertex_policy|surface_model|surface_type|blend|pass|schema|def_hint|vs_features|fs_features|resources|textures|runtime_transition|explicit_axes|legacy_inference|prune_summary|sfm_inferred|sfm_vs_row_resources\n";

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
        out += GetVertexTransformPolicyName(row.vertex_policy);
        out += "|";
        out += GetSurfaceShadingModelName(row.surface_model);
        out += "|";
        out += std::to_string(static_cast<uint32>(row.surface_type));
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
            append("Transform", res.needs_transform);
            append("MaterialInstance", res.needs_material_instance);
            append("MaterialTextureIndex", res.needs_material_texture_index);
            append("Sky", res.needs_sky);
            append("Lighting", res.enable_lighting);
            return first ? std::string("None") : text;
        };

        auto format_textures = [](const MaterialVariantRow &audit_row)
        {
            if (audit_row.color_sources.empty())
                return std::string("None");

            std::string text;
            for (size_t i = 0; i < audit_row.color_sources.size(); ++i)
            {
                if (i > 0)
                    text += ",";
                const auto &cs = audit_row.color_sources[i];
                text += SamplerSlotNameList[static_cast<size_t>(cs.slot)];
                text += ":kind=";
                text += std::to_string(static_cast<int>(cs.kind));
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
            legacy += legacy.empty() ? "lighting_model is ECS-injected via MaterialVariantKey" : ",lighting_model is ECS-injected via MaterialVariantKey";
        if (!row.color_sources.empty())
            legacy += legacy.empty() ? "texture source and sampler bits still mirrored in key but now treated as strict row parity" : ",texture source and sampler bits still mirrored in key but now treated as strict row parity";
        if (RowDeclaresAnyVertexAttrib(row))
            legacy += legacy.empty() ? "vertex_attribute_feature_bits remain runtime-additive but are now constrained to the row-declared attrib envelope" : ",vertex_attribute_feature_bits remain runtime-additive but are now constrained to the row-declared attrib envelope";
        legacy += legacy.empty() ? "extra_feature_bits/effective_feature_mask are now treated as legacy override channels and should stay off builtin row-driven paths" : ",extra_feature_bits/effective_feature_mask are now treated as legacy override channels and should stay off builtin row-driven paths";
        if (row.vertex_policy == VertexTransformPolicy::BillboardCameraFacing || row.vertex_policy == VertexTransformPolicy::BillboardAxisLocked)
            legacy += legacy.empty() ? "billboard behavior still depends on dedicated row template path" : ",billboard behavior still depends on dedicated row template path";
        if (row.vertex_policy == VertexTransformPolicy::TerrainGrid)
            legacy += legacy.empty() ? "terrain behavior still depends on dedicated row template path" : ",terrain behavior still depends on dedicated row template path";
        if (row.vertex_policy == VertexTransformPolicy::Sky)
            legacy += legacy.empty() ? "sky fallback still exists in key-based path" : ",sky fallback still exists in key-based path";
        legacy += legacy.empty() ? "custom descriptors should prefer CreateBuiltinRowBoundVariantDesc() or MaterialVariantDesc::CreateRowBound()/BindRow() over legacy key fallback" : ",custom descriptors should prefer CreateBuiltinRowBoundVariantDesc() or MaterialVariantDesc::CreateRowBound()/BindRow() over legacy key fallback";
        out += legacy.empty() ? "None" : legacy;
        out += "|";

        // Phase 4: per-pass effective resource summary (pruning table applied).
        // Format: pass:sky/light/mi/mitex where 1=kept, 0=pruned.
        auto format_prune_summary = [&](const PassType pass)
        {
            const MaterialResourceRequirements eff = DeriveEffectiveResources(row.resources, pass);
            std::string s;
            s += std::to_string(static_cast<unsigned>(pass));
            s += ":sky=";
            s += eff.needs_sky ? "1" : "0";
            s += "/light=";
            s += eff.enable_lighting ? "1" : "0";
            s += "/mi=";
            s += eff.needs_material_instance ? "1" : "0";
            s += "/mitex=";
            s += eff.needs_material_texture_index ? "1" : "0";
            return s;
        };

        out += format_prune_summary(PassType::ForwardOpaque);
        out += ";";
        out += format_prune_summary(PassType::ShadowOpaque);
        out += ";";
        out += format_prune_summary(PassType::ShadowMasked);
        out += ";";
        out += format_prune_summary(PassType::EarlyZSolid);
        out += ";";
        out += format_prune_summary(PassType::EarlyZMasked);
        out += "|";

        // Phase 6 (P6-1): SFM-inferred resource column.
        // Run SFM assembly for this row (without compiling GLSL) to infer which
        // descriptor sets the assembled shaders actually require, then compare
        // against row.resources to detect MISMATCH.
        {
            mtl::MaterialVariantKey audit_key{};
            audit_key.blend_mode        = row.blend;
            audit_key.lighting_model    = LightingModel::Lambert; // default; ECS injects real value at runtime
            audit_key.sky_ambient_model = SkyLightAmbientModel::Simple; // default
            audit_key.surface_type      = row.surface_type;
            audit_key.pass_hint         = row.pass;
            audit_key.position_provider = row.position_provider;

            const mtl::MaterialVariantDesc audit_desc =
                mtl::MaterialVariantDesc::CreateRowBound(row.name ? row.name : "", &row);

            const auto vs_art = sfm_assembler.AssembleVertexArtifact(audit_key, audit_desc, &row);
            const auto fs_art = sfm_assembler.AssembleFragmentArtifact(audit_key, audit_desc, &row);

            hgl::graph::ShaderRequirementSet merged;
            merged.MergeFrom(vs_art.req_set);
            merged.MergeFrom(fs_art.req_set);

            const bool sfm_viewport   = merged.Requires("viewport");
            const bool sfm_camera     = merged.Requires("camera");
            const bool sfm_transform  = merged.Requires("transform_id")
                                     || merged.Requires("transform_data");

            // Format sfm_inferred column.
            {
                std::string sfm_txt;
                auto add = [&](const char *n, bool v)
                {
                    if (!v) return;
                    if (!sfm_txt.empty()) sfm_txt += ",";
                    sfm_txt += n;
                };
                add("Viewport",   sfm_viewport);
                add("Camera",     sfm_camera);
                add("Transform",  sfm_transform);
                if (!vs_art.success) { sfm_txt += sfm_txt.empty() ? "VS_FAIL" : ",VS_FAIL"; }
                if (!fs_art.success) { sfm_txt += sfm_txt.empty() ? "FS_FAIL" : ",FS_FAIL"; }
                out += sfm_txt.empty() ? "None" : sfm_txt;
            }
            out += "|";

            // Format sfm_vs_row_resources mismatch column.
            {
                std::string mismatch;
                auto check = [&](const char *field, bool sfm_val, bool row_val)
                {
                    if (sfm_val == row_val) return;
                    if (!mismatch.empty()) mismatch += ",";
                    mismatch += field;
                    mismatch += ":sfm=";
                    mismatch += sfm_val ? "1" : "0";
                    mismatch += "/row=";
                    mismatch += row_val ? "1" : "0";
                };
                check("viewport",   sfm_viewport,  row.resources.needs_viewport);
                check("camera",     sfm_camera,    row.resources.needs_camera);
                check("transform",  sfm_transform, row.resources.needs_transform);
                out += mismatch.empty() ? "OK" : ("MISMATCH:" + mismatch);
            }
        }
        out += "\n";
    });

    return out;
}

const MaterialVariantRow *FindBuiltinMaterialVariantRowByName(const char *name)
{
    if (!name || !name[0])
        return nullptr;

    const auto &registry = GetBuiltinVariantRegistry();
    const MaterialVariantRow *result = nullptr;

    registry.ForEachBuiltinRow([&](const MaterialVariantRow &row)
    {
        if (result)
            return;

        if (row.name && std::strcmp(row.name, name) == 0)
            result = &row;
    });

    return result;
}

MaterialVariantDesc CreateBuiltinRowBoundVariantDesc(const char *row_name,
                                                     const std::optional<MaterialPreset> &type,
                                                     const std::string &vs_path,
                                                     const std::string &fs_path,
                                                     const std::string &surface_path)
{
    const MaterialVariantRow *row = FindBuiltinMaterialVariantRowByName(row_name);

    if (!row)
    {
        std::fprintf(stderr,
                     "[MaterialLibrary] warning: CreateBuiltinRowBoundVariantDesc failed to find builtin row '%s'. "
                     "The returned descriptor has no bound_row; prefer a valid builtin row name or explicit MaterialVariantDesc::CreateRowBound().\n",
                     row_name ? row_name : "<null>");
    }

    return MaterialVariantDesc::CreateRowBound(row_name ? row_name : "", row, type, vs_path, fs_path, surface_path);
}

namespace {

static std::string FormatVariantKeyForLog(const MaterialVariantKey &key)
{
    std::string text;
    text.reserve(352);

    text += "hash=";
    text += std::to_string(static_cast<unsigned long long>(key.Hash()));
    text += " row=0x";

    char hex64[24] = {};
    std::snprintf(hex64, sizeof(hex64), "%016llX",
        static_cast<unsigned long long>(key.variant_row_name_hash));
    text += hex64;
    text += " ST=";
    text += std::to_string(static_cast<unsigned>(key.surface_type));
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
    // One canonical name per semantic preset.
    // 2D vs 3D is a vertex transform policy, not a preset distinction.
    {MaterialPreset::Checkerboard3D,       "Checkerboard3D"},
    {MaterialPreset::VertexColor,          "VertexColor"},
    {MaterialPreset::PureColor,            "PureColor"},
    {MaterialPreset::UnlitTexture,         "UnlitTexture"},
    {MaterialPreset::Text2D,               "Text2D"},
    {MaterialPreset::VertexLuminance,      "VertexLuminance"},
    {MaterialPreset::VertexPaletteColor3D, "VertexPaletteColor3D"},
    {MaterialPreset::Gizmo3D,              "Gizmo3D"},
    {MaterialPreset::TerrainGrid,          "TerrainGrid"},
    {MaterialPreset::SkyMinimal,           "SkyMinimal"},
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

static void StripLegacyOverrideChannelsForBuiltinPreset(const MaterialPreset preset,
                                                        MaterialVariantKey &key)
{
    if (key.extra_feature_bits == 0 && key.effective_feature_mask == 0)
        return;

    static std::atomic_bool s_warned{false};
    bool expected = false;
    if (s_warned.compare_exchange_strong(expected, true, std::memory_order_relaxed))
    {
        std::fprintf(stderr,
                     "[MaterialLibrary] warning: builtin preset route preset=%u carried legacy override channels "
                     "(extra_bits=0x%08X, effective_feature_mask=0x%016llX). Clearing them to keep builtin row-driven routing on explicit row identity.\n",
                     static_cast<unsigned>(preset),
                     key.extra_feature_bits,
                     static_cast<unsigned long long>(key.effective_feature_mask));
    }

    key.extra_feature_bits = 0;
    key.effective_feature_mask = 0;
}

static void NormalizeBuiltinPresetParityOverrides(const MaterialPreset preset,
                                                  const MaterialCreateConfig *cfg,
                                                  const MaterialVariantKey &routed_key,
                                                  MaterialVariantKey &key)
{
    const auto *cfg3d = As3D(cfg);
    if (!cfg3d)
        return;

    if (preset == MaterialPreset::PBRColor3D)
        return;

    if (cfg3d->lighting_model != routed_key.lighting_model)
    {
        std::fprintf(stderr,
                     "[MaterialLibrary] warning: ignoring lighting_model override=%u for builtin preset=%u; builtin row-driven path requires strict parity with routed key=%u.\n",
                     static_cast<unsigned>(cfg3d->lighting_model),
                     static_cast<unsigned>(preset),
                     static_cast<unsigned>(routed_key.lighting_model));
    }

    if (cfg3d->sky_ambient_model != routed_key.sky_ambient_model)
    {
        std::fprintf(stderr,
                     "[MaterialLibrary] warning: ignoring sky_ambient_model override=%u for builtin preset=%u; builtin row-driven path requires strict parity with routed key=%u.\n",
                     static_cast<unsigned>(cfg3d->sky_ambient_model),
                     static_cast<unsigned>(preset),
                     static_cast<unsigned>(routed_key.sky_ambient_model));
    }

    key.lighting_model = routed_key.lighting_model;
    key.sky_ambient_model = routed_key.sky_ambient_model;
}

/// Phase 4 (收尾): Row-aware texture override check — replaces the old preset allowlist.
/// Accepts the override only when the overridden key resolves to a registered builtin
/// variant AND that variant has a bound row in kBuiltinVariantRows.  This rule applies
/// to all presets uniformly; no special-cased allowlist is needed.
static bool IsBuiltinTextureOverrideAllowedForKey(const MaterialPreset resolved_preset,
                                                   const MaterialVariantKey &overridden_key) noexcept
{
    const RegistryLookupOptions opts{};
    const MaterialVariantDesc *desc = GetBuiltinVariantRegistry().QueryVariant(overridden_key, opts);
    if (!desc || !desc->factory_type || *desc->factory_type != resolved_preset)
        return false;

    return FindBuiltinMaterialVariantRowByName(desc->variant_name.c_str()) != nullptr;
}

static void NormalizeBuiltinPresetTextureOverrides(const MaterialPreset preset,
                                                   const MaterialCreateConfig *cfg,
                                                   const MaterialVariantKey &routed_key,
                                                   MaterialVariantKey &key)
{
    if (!cfg)
        return;

    if (!cfg->HasTextureSourceBitsOverride() && cfg->sampler_feature_bits_override == 0)
        return;

    if (IsBuiltinTextureOverrideAllowedForKey(preset, key))
        return;

    std::fprintf(stderr,
                 "[MaterialLibrary] warning: rejecting texture/sampler override for preset=%u; "
                 "the overridden key does not resolve to a registered builtin row — "
                 "keeping texture_source_bits/sampler_feature_bits at routed parity.\n",
                 static_cast<unsigned>(preset));

    key.texture_source_bits  = routed_key.texture_source_bits;
    key.sampler_feature_bits = routed_key.sampler_feature_bits;
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

    // Step 2: collect all registry entries whose factory_type matches resolved_preset,
    //   then select the first one (by builtin_row_storage insertion order, recovered via
    //   ascending bound_row pointer) that passes the hard filters.
    //
    //   • ov.position_provider is a runtime VS-only axis written directly into the key
    //     (Step 5 below). It does NOT filter registry rows.
    //   • ov.preferred_vertex_policy is a *hard* filter (billboard presets only).
    //   • blend_mode and lighting_model are *hard* filters (always applied).
    //   • First match wins; candidates are sorted by bound_row address (ascending) which
    //     equals insertion order in builtin_row_storage (contiguous vector).
    struct Candidate { const MaterialVariantRow *row; MaterialVariantKey key; };
    std::vector<Candidate> candidates;
    GetBuiltinVariantRegistry().ForEach([&](const MaterialVariantKey &k, const MaterialVariantDesc &desc) {
        if (desc.factory_type && *desc.factory_type == resolved_preset && desc.bound_row)
            candidates.push_back({desc.bound_row, k});
    });
    // Sort by insertion order (bound_row lives in a contiguous vector; lower address = earlier entry).
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate &a, const Candidate &b) { return a.row < b.row; });

    const Candidate *found = nullptr;
    for (const auto &c : candidates)
    {
        if (ov.preferred_vertex_policy && c.row->vertex_policy != *ov.preferred_vertex_policy) continue;
        if (ov.blend_mode     && c.key.blend_mode     != *ov.blend_mode)                       continue;
        if (ov.lighting_model && c.key.lighting_model != *ov.lighting_model)                   continue;
        found = &c;
        break;
    }

    if (!found)
    {
        std::fprintf(stderr,
            "[MaterialLibrary] ERROR: RouteKey no builtin entry for preset=%u\n",
            static_cast<unsigned>(preset));
        return MaterialVariantKey{};
    }

    // Step 3: take the pre-built key from the matched registry entry.
    MaterialVariantKey key = found->key;

    // Step 4: OR-merge caller-supplied extra vertex attribute bits.
    key.vertex_attribute_feature_bits |= extra_attrib_bits;
    key.vertex_attribute_feature_bits |= ov.extra_vertex_attrib_bits;

    // Step 5: apply remaining overrides (not covered by entry selection).
    if (ov.position_provider) key.position_provider = *ov.position_provider;  // Step 11.D
    if (ov.pass_hint)         key.pass_hint         = *ov.pass_hint;
    if (ov.sky_ambient_model) key.sky_ambient_model = *ov.sky_ambient_model;

    return key;
}



uint64 ResolveBuiltinVariantRowHash(MaterialPreset preset,
                                    const MaterialVariantKey &key) noexcept
{
    const MaterialPreset resolved_preset =
        ResolveMaterialPresetForLOD(preset, GetDefaultMaterialLOD());

    MaterialVariantKey query = key;
    query.variant_row_name_hash = 0;
    query.effective_feature_mask = 0;

    // sky_ambient_model is never a routing axis: canonicalize to Simple for all presets.
    query.sky_ambient_model = SkyLightAmbientModel::Simple;

    // Collect and sort candidates by insertion order (same bound_row pointer trick as RouteKey).
    struct Candidate { const MaterialVariantRow *row; MaterialVariantKey key; };
    std::vector<Candidate> candidates;
    GetBuiltinVariantRegistry().ForEach([&](const MaterialVariantKey &k, const MaterialVariantDesc &desc) {
        if (desc.factory_type && *desc.factory_type == resolved_preset && desc.bound_row)
            candidates.push_back({desc.bound_row, k});
    });
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate &a, const Candidate &b) { return a.row < b.row; });

    for (const auto &c : candidates)
    {
        MaterialVariantKey candidate = c.key;
        const uint64 candidate_row_hash = candidate.variant_row_name_hash;
        candidate.variant_row_name_hash = 0;
        candidate.effective_feature_mask = 0;
        candidate.sky_ambient_model = SkyLightAmbientModel::Simple;

        if (candidate == query)
            return candidate_row_hash;
    }

    return key.variant_row_name_hash;
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
    // [Step 3.5 T4] Routing self-test: every registered builtin variant must round-trip
    // through the registry. Any mismatch is a programming error → abort immediately.
    static const bool s_routing_consistency_ok = []() noexcept
    {
        bool all_ok = true;
        size_t entry_count = 0;
        GetBuiltinVariantRegistry().ForEach(
            [&](const MaterialVariantKey &k, const MaterialVariantDesc &desc)
            {
                ++entry_count;
                if (!desc.factory_type.has_value())
                {
                    std::fprintf(stderr,
                        "[MaterialLibrary] FATAL: routing self-test FAILED for entry[%zu] \"%s\""
                        " (missing factory_type)\n",
                        entry_count - 1,
                        desc.variant_name.c_str());
                    all_ok = false;
                    return;
                }

                RegistryLookupOptions routing_opts{};
                routing_opts.preferred_factory_type = *desc.factory_type;
                const MaterialVariantDesc *found =
                    GetBuiltinVariantRegistry().QueryVariantWithCanonicalFallback(k, nullptr, routing_opts);
                const bool entry_ok = found
                                    && found->factory_type.has_value()
                                    && *found->factory_type == *desc.factory_type;
                if (!entry_ok)
                {
                    std::fprintf(stderr,
                        "[MaterialLibrary] FATAL: routing self-test FAILED for entry[%zu] \"%s\""
                        " (preset=%u): registry returned %s\n",
                        entry_count - 1,
                        desc.variant_name.c_str(),
                        static_cast<unsigned>(*desc.factory_type),
                        found ? (found->factory_type.has_value()
                                 ? "wrong factory_type"
                                 : "desc with no factory_type")
                              : "nullptr");
                    all_ok = false;
                }
            });
        if (!all_ok)
        {
            std::fprintf(stderr,
                "[MaterialLibrary] FATAL: BuiltinVariantEntry routing self-test failed"
                " — aborting to prevent undefined behaviour in main loop.\n");
            std::abort();
        }
        std::printf("[MaterialLibrary] BuiltinVariantEntry routing self-test passed"
                    " (%zu entries).\n", entry_count);
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
            "[MaterialLibrary] CreateMaterialCreateInfo warning: profile is null (key_hash=%llu surface=%u tex_mode=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X)\n",
            static_cast<unsigned long long>(key.Hash()),
            static_cast<unsigned>(key.surface_type),
            static_cast<unsigned>(key.GetTextureSourceMode(SamplerSlot::BaseColor)),
            key.texture_source_bits,
            key.sampler_feature_bits,
            key.vertex_attribute_feature_bits,
            key.extra_feature_bits);
    }

    MaterialVariantKey registry_lookup_key = key;
    {
        // Phase 3: use table-driven sky identity-axis rules.
        // Determine resolved preset for the lookup key so we can query the table.
        MaterialPreset lookup_preset = MaterialPreset::Standard; // fallback
        if (cfg->preset_name)
        {
            for (const auto &e : kPresetResolveTable)
            {
                if (std::strcmp(cfg->preset_name, e.name) == 0)
                {
                    lookup_preset = ResolveMaterialPresetForLOD(e.preset, GetDefaultMaterialLOD());
                    break;
                }
            }
        }
        // sky_ambient_model is never a routing axis: always canonicalize to Simple for registry lookup.
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

    MaterialVariantRow phase3_row{};
    MaterialVariantDesc phase3_desc{};

    const MaterialVariantDesc *variant_desc = GetBuiltinVariantRegistry().QueryVariantWithCanonicalFallback(registry_lookup_key,
                                                                                                             &resolved_key,
                                                                                                             lookup_opts);
    if(!variant_desc)
    {
        std::fprintf(stderr,
            "[MaterialLibrary] CreateMaterialCreateInfo failed: no registered variant"
            " (key_hash=%llu surface=%u sky=%u tex_mode=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X)"
            " [Phase 3: sky canonicalized to Simple=%s]\n",
            static_cast<unsigned long long>(key.Hash()),
            static_cast<unsigned>(key.surface_type),
            static_cast<unsigned>(key.sky_ambient_model),
            static_cast<unsigned>(key.GetTextureSourceMode(SamplerSlot::BaseColor)),
            key.texture_source_bits,
            key.sampler_feature_bits,
            key.vertex_attribute_feature_bits,
            key.extra_feature_bits,
            registry_lookup_key.sky_ambient_model == SkyLightAmbientModel::Simple
                && key.sky_ambient_model != SkyLightAmbientModel::Simple ? "yes" : "no");

        // Phase 3: before dropping into Phase 2's geometry-only FS fallback, try the
        // new three-table query path and compose a temporary legacy row for the existing
        // assembler / factory pipeline.
        const RegistryQueryResult phase3_query = QueryPhase3Registry(registry_lookup_key);
        if (phase3_query.vertex && phase3_query.fragment && phase3_query.pipeline)
        {
            phase3_row = ComposeMaterialVariantRow(phase3_query.vertex,
                                          phase3_query.fragment,
                                          phase3_query.pipeline,
                                          key,
                                          "ComposedRow");

            phase3_desc = MaterialVariantDesc::CreateRowBound(
                "Phase3ComposedVariant",
                &phase3_row,
                phase3_query.fragment->preset,
                phase3_query.vertex->vs_template_path ? phase3_query.vertex->vs_template_path : "",
                phase3_query.fragment->fs_template_path ? phase3_query.fragment->fs_template_path : "",
                phase3_query.fragment->surface_path ? phase3_query.fragment->surface_path : "");

            variant_desc = &phase3_desc;
            resolved_key = key;

            std::fprintf(stderr,
                "[MaterialLibrary] Phase3 route hit: vertex=%s fragment=%s pipeline=%s request={%s}\n",
                phase3_query.vertex->name,
                phase3_query.fragment->name,
                phase3_query.pipeline->name,
                FormatVariantKeyForLog(key).c_str());
        }
    }

    if(!variant_desc)
    {

        // Phase 2: VS/FS split fallback.
        // The full variant miss does not necessarily mean VS is broken — it may be
        // that only the FS surface configuration (tex_bits / sampler_bits) has no match.
        // Strategy: scan registered variants for one that shares the same surface_type.
        // If found, VS assembly is likely valid; substitute ErrorIndicator surface for FS.
        const MaterialVariantDesc *vs_candidate = nullptr;
        MaterialVariantKey vs_candidate_key{};
        GetBuiltinVariantRegistry().ForEach(
            [&](const MaterialVariantKey &vk, const MaterialVariantDesc &vd)
            {
                if (vs_candidate)
                    return; // already found one
                if (!vd.factory_type)
                    return;
                vs_candidate     = &vd;
                vs_candidate_key = vk;
            });

        if (vs_candidate && vs_candidate->factory_type)
        {
            // Encode the FS error reason so the user can see why the surface failed.
            const uint32_t error_code = EncodeFSError(
                FSErrorReason::NoSurfaceVariant,
                static_cast<uint8_t>(key.surface_type),
                static_cast<uint8_t>(key.texture_source_bits & 0xFF),
                static_cast<uint8_t>(key.sampler_feature_bits & 0xFF));

            std::fprintf(stderr,
                "[MaterialLibrary] Phase2 FS-fallback: VS candidate='%s'"
                " error_code=0x%08X [%s]\n",
                vs_candidate->variant_name.c_str(),
                error_code,
                FormatFSError(error_code).c_str());

            // Clone the candidate desc and set ErrorIndicator surface + error code.
            MaterialVariantDesc ei_desc = *vs_candidate;
            if (vs_candidate->bound_row && vs_candidate->bound_row->name)
            {
                // Keep variant_name aligned with bound_row identity so builtin
                // row-consistency validation passes in the assembler.
                ei_desc.variant_name = vs_candidate->bound_row->name;
            }
            else
            {
                ei_desc.variant_name = vs_candidate->variant_name;
            }
            ei_desc.surface_function_path = "surface/error_indicator_surface.glsl";
            ei_desc.fs_template_path.clear();
            ei_desc.fs_error_code       = error_code;

            // Build a fallback key aligned to the selected VS candidate row identity
            // so bound_row structural validation can pass in the assembler.
            MaterialVariantKey fallback_key = key;
            fallback_key.surface_type      = vs_candidate_key.surface_type;
            fallback_key.position_provider = vs_candidate_key.position_provider;
            fallback_key.blend_mode        = vs_candidate_key.blend_mode;
            fallback_key.pass_hint         = vs_candidate_key.pass_hint;

            if (MaterialCreateInfo *ei_mci = MaterialFactory3D::Create(
                    *vs_candidate->factory_type,
                    profile,
                    &ei_desc,
                    fallback_key,
                    cfg))
            {
                std::fprintf(stderr,
                    "[MaterialLibrary] Phase2 FS-fallback applied key_hash=0x%llx variant='%s' fs='error_indicator_surface.glsl' error_code=0x%08X\n",
                    static_cast<unsigned long long>(key.Hash()),
                    ei_desc.variant_name.c_str(),
                    error_code);
                return ei_mci;
            }

            std::fprintf(stderr,
                "[MaterialLibrary] Phase2 FS-fallback failed: factory dispatch returned null"
                " (candidate='%s')\n",
                vs_candidate->variant_name.c_str());
        }
        else
        {
            std::fprintf(stderr,
                "[MaterialLibrary] Phase2 FS-fallback: no matching candidate found"
                " — VS routing also failed\n");
        }

        return nullptr;
    }

    std::fprintf(stderr,
        "[MaterialLibrary] resolved variant=%s request={%s} lookup={%s} resolved={%s}\n",
        variant_desc->variant_name.c_str(),
        FormatVariantKeyForLog(key).c_str(),
        FormatVariantKeyForLog(registry_lookup_key).c_str(),
        FormatVariantKeyForLog(resolved_key).c_str());

    // Phase 4: derive effective resource requirements by applying pass/quality pruning.
    // The bound_row carries the full authored resource policy; pruning narrows it for
    // shadow/early-z/low-quality variants. This result is diagnostic-only for now (Phase 5
    // will wire it into the descriptor builder).
    if (variant_desc->bound_row)
    {
        const MaterialResourceRequirements &authored = variant_desc->bound_row->resources;
        const PassType pass_hint = key.pass_hint;
        const MaterialResourceRequirements effective = DeriveEffectiveResources(authored, pass_hint);

        const bool sky_pruned    = authored.needs_sky        && !effective.needs_sky;
        const bool light_pruned   = authored.enable_lighting  && !effective.enable_lighting;
        const bool mi_pruned      = authored.needs_material_instance && !effective.needs_material_instance;
        const bool mitex_pruned   = authored.needs_material_texture_index && !effective.needs_material_texture_index;

        if (sky_pruned || light_pruned || mi_pruned || mitex_pruned)
        {
            std::fprintf(stderr,
                "[MaterialLibrary] Phase4 prune: variant=%s pass=%u"
                " sky=%d->%d light=%d->%d mi=%d->%d mitex=%d->%d\n",
                variant_desc->variant_name.c_str(),
                static_cast<unsigned>(pass_hint),
                authored.needs_sky,        effective.needs_sky,
                authored.enable_lighting,  effective.enable_lighting,
                authored.needs_material_instance,       effective.needs_material_instance,
                authored.needs_material_texture_index,  effective.needs_material_texture_index);
        }
    }

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

    if (const auto *cfg3d = As3D(cfg))
    {
        key.sky_ambient_model = cfg3d->sky_ambient_model;
        key.lighting_model = cfg3d->lighting_model;
        
        // Phase 2: Copy effective feature mask (resolved from intent_features)
        // If non-zero, this is the authoritative feature decision.
        key.effective_feature_mask = cfg3d->effective_feature_mask;
    }

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
    const MaterialVariantKey routed_key = RouteKey(resolved_preset);
    MaterialVariantKey key = routed_key;

    ApplyCreateConfigToVariantKey(key, cfg);
    StripLegacyOverrideChannelsForBuiltinPreset(resolved_preset, key);
    NormalizeBuiltinPresetParityOverrides(resolved_preset, cfg, routed_key, key);
    NormalizeBuiltinPresetTextureOverrides(resolved_preset, cfg, routed_key, key);

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
