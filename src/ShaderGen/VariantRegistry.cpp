#include<hgl/mtl/MaterialVariantRegistry.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/shadergen/RegistryQuery.h>
#include<hgl/shadergen/ShaderResourceScanner.h>
#include "BuiltinVariantEntry.h"
#include <hgl/mtl/MaterialVariantRow.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <initializer_list>
#include <string>

namespace hgl::graph::mtl{
namespace
{
#if defined(ULRE_SHADERGEN_VERBOSE)
constexpr bool kVariantRegistryVerbose = true;
#else
constexpr bool kVariantRegistryVerbose = false;
#endif

static MaterialVariantKey CanonicalizeRegistryLookupKey(const MaterialVariantKey &key,
                                                        const RegistryLookupOptions &options)
{
    MaterialVariantKey canon = key;

    if (!options.match_effective_feature_mask && canon.effective_feature_mask != 0)
    {
        static std::atomic_bool s_warned_effective_mask_ignored{false};
        bool expected = false;
        if (s_warned_effective_mask_ignored.compare_exchange_strong(expected, true, std::memory_order_relaxed))
        {
            std::fprintf(stderr,
                "[VariantRegistry] warning: effective_feature_mask is ignored for variant routing by default; "
                "set RegistryLookupOptions::match_effective_feature_mask = true to include it in registry lookup.\n");
        }

        canon.effective_feature_mask = 0;
    }

    return canon;
}

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
    text += " GM=";
    text += std::to_string(static_cast<unsigned>(key.geometry_mode));
    text += " blend=";
    text += std::to_string(static_cast<unsigned>(key.blend_mode));
    text += " pass=";
    text += std::to_string(static_cast<unsigned>(key.pass_hint));
    text += " sky=";
    text += std::to_string(static_cast<unsigned>(key.sky_ambient_model));
    text += " light=";
    text += std::to_string(static_cast<unsigned>(key.lighting_model));
    text += " eff=0x";

    std::snprintf(hex64, sizeof(hex64), "%016llX",
        static_cast<unsigned long long>(key.effective_feature_mask));
    text += hex64;
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

static std::string FormatShaderStageFeatureDescForLog(const ShaderStageFeatureDesc &desc)
{
    std::string text;
    bool first = true;
    for (size_t i = 0; i < static_cast<size_t>(hgl::graph::VertexAttrib::RANGE_SIZE); ++i)
    {
        const auto attrib = static_cast<hgl::graph::VertexAttrib>(i);
        if (!desc.HasVertexAttrib(attrib))
            continue;

        if (!first)
            text += ",";

        text += GetVertexAttribName(attrib);
        text += "=1";
        first = false;
    }

    if (first)
        text = "None";

    text += ",dir=";
    text += desc.has_direction ? "1" : "0";
    text += ",clip=";
    text += desc.has_clip_pos ? "1" : "0";
    return text;
}

static std::string FormatMaterialResourcesForLog(const MaterialResourceRequirements &res)
{
    std::string text;
    text += "VP=";
    text += res.needs_viewport ? "1" : "0";
    text += ",Cam=";
    text += res.needs_camera ? "1" : "0";
    text += ",Sky=";
    text += res.needs_sky ? "1" : "0";
    text += ",Xf=";
    text += res.needs_transform ? "1" : "0";
    text += ",MI=";
    text += res.needs_material_instance ? "1" : "0";
    text += ",MITex=";
    text += res.needs_material_texture_index ? "1" : "0";
    text += ",Pal=";
    text += res.needs_color_palette ? "1" : "0";
    text += ",Lit=";
    text += res.enable_lighting ? "1" : "0";
    text += ",LM=";
    text += std::to_string(static_cast<unsigned>(res.lighting_model));
    text += ",SkyModel=";
    text += std::to_string(static_cast<unsigned>(res.sky_model));
    return text;
}

static std::string FormatRowTexturesForLog(const MaterialVariantRow &row)
{
    std::string text;
    if (row.texture_count == 0)
        return "None";

    for (uint32 i = 0; i < row.texture_count; ++i)
    {
        if (i > 0)
            text += ",";

        const auto &t = row.textures[i];
        text += SamplerSlotNameList[static_cast<size_t>(t.slot)];
        text += ":src=";
        text += std::to_string(static_cast<unsigned>(t.source_mode));
        text += ":smp=";
        text += std::to_string(static_cast<unsigned>(t.sampler_type));
    }

    return text;
}

static ShaderStageFeatureDesc MakeStageFeatures(std::initializer_list<hgl::graph::VertexAttrib> attribs,
                                                const bool has_direction = false,
                                                const bool has_clip_pos = false)
{
    ShaderStageFeatureDesc desc;
    for (const auto attrib : attribs)
        desc.SetVertexAttrib(attrib);
    desc.has_direction = has_direction;
    desc.has_clip_pos = has_clip_pos;
    return desc;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// VariantRegistry
// ---------------------------------------------------------------------------

void VariantRegistry::RegisterVariant(const MaterialVariantKey &key, const MaterialVariantDesc &desc)
{
    const uint64 hash = key.Hash();
    auto &bucket = variant_map[hash];

    for (const auto &existing : bucket)
    {
        if (!(existing.key == key))
            continue;

        const bool same_factory = existing.desc.factory_type == desc.factory_type;
        if (same_factory)
        {
            std::fprintf(stderr,
                "[VariantRegistry] duplicate variant ignored on RegisterVariant '%s' (hash=0x%016llx).\n",
                desc.variant_name.empty() ? "<unnamed>" : desc.variant_name.c_str(),
                static_cast<unsigned long long>(hash));
            return;
        }
    }

    bucket.push_back(VariantEntry{key, desc});
    ++variant_count;
}

const MaterialVariantDesc *VariantRegistry::QueryVariant(const MaterialVariantKey &key,
                                                         const RegistryLookupOptions &options) const
{
    const MaterialVariantKey query_key = CanonicalizeRegistryLookupKey(key, options);

    auto it = variant_map.find(query_key.Hash());
    if (it == variant_map.end())
        return nullptr;

    const auto *fallback = static_cast<const MaterialVariantDesc *>(nullptr);

    for (const auto &candidate : it->second)
    {
        if (!(candidate.key == query_key))
            continue;

        if (options.preferred_factory_type
         && candidate.desc.factory_type
         && *candidate.desc.factory_type == *options.preferred_factory_type)
        {
            return &candidate.desc;
        }

        if (!fallback)
            fallback = &candidate.desc;
    }

    return fallback;
}

const MaterialVariantDesc *VariantRegistry::QueryVariantWithCanonicalFallback(
    const MaterialVariantKey &key,
    MaterialVariantKey *resolved_key,
    const RegistryLookupOptions &options) const
{
    const MaterialVariantKey request_key = CanonicalizeRegistryLookupKey(key, options);

  if (kVariantRegistryVerbose && !(request_key == key))
  {
    std::fprintf(stderr,
      "[VariantRegistry] canonicalized lookup request={%s} canonical={%s}\n",
      FormatVariantKeyForLog(key).c_str(),
      FormatVariantKeyForLog(request_key).c_str());
  }

    if(const MaterialVariantDesc *exact=QueryVariant(request_key, options))
    {
        if (auto *s = GetGlobalVariantRegistryStatsSink())
            s->OnExactMatch(request_key, *exact);
        if (kVariantRegistryVerbose)
        {
            std::fprintf(stderr,
                "[VariantRegistry] exact-match variant=%s %s\n",
                exact->variant_name.empty() ? "<unnamed>" : exact->variant_name.c_str(),
                FormatVariantKeyForLog(request_key).c_str());
        }
        if(resolved_key)
            *resolved_key=request_key;
        return exact;
    }

    if (auto *s = GetGlobalVariantRegistryStatsSink())
        s->OnMiss(request_key);

    std::fprintf(stderr,
      "[VariantRegistry] miss request={%s}\n",
      FormatVariantKeyForLog(request_key).c_str());

    // Diagnostic: enumerate all registered variants that share the same preset/surface/geom
    // and show what differs so the caller can understand why lookup failed.
    {
        std::optional<MaterialPreset> req_preset = options.preferred_factory_type;
        int close_count = 0;
        for (const auto &[hash, bucket] : variant_map)
        {
            for (const auto &entry : bucket)
            {
                bool same_preset = !req_preset || (entry.desc.factory_type && *entry.desc.factory_type == *req_preset);
                bool same_surface = (entry.key.surface_type == request_key.surface_type);
                bool same_geom    = (entry.key.geometry_mode == request_key.geometry_mode);
                if (!same_preset || !same_surface || !same_geom)
                    continue;
                if (close_count == 0)
                    std::fprintf(stderr, "[VariantRegistry] miss candidates (same preset/surface/geom):\n");
                std::fprintf(stderr,
                    "[VariantRegistry]   candidate variant='%s' tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X"
                    " (request tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X)\n",
                    entry.desc.variant_name.c_str(),
                    entry.key.texture_source_bits,
                    entry.key.sampler_feature_bits,
                    entry.key.vertex_attribute_feature_bits,
                    request_key.texture_source_bits,
                    request_key.sampler_feature_bits,
                    request_key.vertex_attribute_feature_bits);
                if (++close_count >= 8) break;
            }
            if (close_count >= 8) break;
        }
        if (close_count == 0)
            std::fprintf(stderr, "[VariantRegistry] miss: no registered variant shares same preset/surface/geom\n");
    }

    return nullptr;
}

std::string VariantRegistry::DumpSnapshot() const
{
    std::vector<std::pair<uint64, const VariantEntry *>> rows;
    rows.reserve(variant_count);

    for(const auto &[hash,bucket]:variant_map)
        for (const auto &entry : bucket)
            rows.emplace_back(hash,&entry);

    std::sort(rows.begin(),rows.end(),
        [](const auto &a,const auto &b)
        {
            return a.first<b.first;
        });

    std::string out;
    out.reserve(rows.size()*120);

    out += "# VariantRegistry Snapshot\n";
    out += "hash|name|factory|surface|geometry|tex_modes|tex_bits|sampler_bits|va_bits|extra_bits|blend|pass|row_vertex_input|row_vertex_policy|row_surface_model|row_vs_features|row_fs_features|row_resources|row_schema|row_def_hint|row_textures\n";

    for(const auto &[hash,entry_ptr]:rows)
    {
        const auto &entry=*entry_ptr;
        const auto &k=entry.key;
        const auto &d=entry.desc;

        const RegistryQueryResult phase3 = QueryPhase3Registry(k);
        const MaterialVariantRow row = (phase3.vertex && phase3.fragment && phase3.pipeline)
            ? ComposeLegacyRow(phase3.vertex, phase3.fragment, phase3.pipeline, k, d.variant_name.c_str())
            : (d.bound_row ? *d.bound_row : MaterialVariantRow{});

        out += std::to_string(hash);
        out += "|";
        out += d.variant_name;
        out += "|";
        out += d.factory_type
            ? std::to_string(static_cast<uint32>(*d.factory_type))
            : std::string("-1");
        out += "|";
        out += std::to_string(static_cast<uint32>(k.surface_type));
        out += "|";
        out += std::to_string(static_cast<uint32>(k.geometry_mode));
        out += "|";
        {
            bool first = true;
            for (size_t i = 0; i < SamplerSlotCount; ++i)
            {
                const SamplerSlot slot = static_cast<SamplerSlot>(i);
                const TextureSourceMode mode = k.GetTextureSourceMode(slot);
                if (mode != TextureSourceMode::None)
                {
                    if (!first)
                        out += ",";
                    out += SamplerSlotNameList[i];
                    out += ":";
                    out += std::to_string(static_cast<uint32>(mode));
                    first = false;
                }
            }
            if (first)
                out += "None";
            out += "|";
        }
        out += std::to_string(k.texture_source_bits);
        out += "|";
        out += std::to_string(k.sampler_feature_bits);
        out += "|";
        out += std::to_string(k.vertex_attribute_feature_bits);
        out += "|";
        out += std::to_string(k.extra_feature_bits);
        out += "|";
        out += std::to_string(static_cast<uint32>(k.blend_mode));
        out += "|";
        out += std::to_string(static_cast<uint32>(k.pass_hint));
        out += "|";
        out += row.name && row.name[0] ? GetVertexInputProfileName(row.vertex_input) : "Unknown";
        out += "|";
        out += row.name && row.name[0] ? GetVertexTransformPolicyName(row.vertex_policy) : "Unknown";
        out += "|";
        out += row.name && row.name[0] ? GetSurfaceShadingModelName(row.surface_model) : "Unknown";
        out += "|";
        out += row.name && row.name[0] ? FormatShaderStageFeatureDescForLog(row.vs_features) : "";
        out += "|";
        out += row.name && row.name[0] ? FormatShaderStageFeatureDescForLog(row.fs_features) : "";
        out += "|";
        out += row.name && row.name[0] ? FormatMaterialResourcesForLog(row.resources) : "";
        out += "|";
        out += row.name && row.name[0] ? std::to_string(static_cast<uint32>(row.schema)) : "0";
        out += "|";
        out += row.name && row.name[0] ? GetStaticMaterialDefIdHintName(row.def_hint) : "None";
        out += "|";
        out += row.name && row.name[0] ? FormatRowTexturesForLog(row) : "None";
        out += "\n";
    }

    return out;
}

std::string VariantRegistry::DumpBuiltinRowSnapshot() const
{
    std::string out;
    out.reserve(variant_count * 240);

    out += "# Builtin MaterialVariantRow Snapshot\n";
    out += "name|preset|factory|primitive|surface|geometry|position_provider|vertex_input|vertex_policy|surface_model|blend|pass|vs_template|fs_template|surface_path|vs_features|fs_features|resources|schema|def_hint|textures\n";

    for(const auto &[hash,bucket]:variant_map)
    {
        for(const auto &entry:bucket)
        {
            const RegistryQueryResult phase3 = QueryPhase3Registry(entry.key);
            const MaterialVariantRow row = (phase3.vertex && phase3.fragment && phase3.pipeline)
                ? ComposeLegacyRow(phase3.vertex, phase3.fragment, phase3.pipeline, entry.key, entry.desc.variant_name.c_str())
                : (entry.desc.bound_row ? *entry.desc.bound_row : MaterialVariantRow{});

            out += row.name ? row.name : "";
            out += "|";
            out += std::to_string(static_cast<uint32>(row.preset));
            out += "|";
            out += std::to_string(static_cast<uint32>(row.factory_type));
            out += "|";
            out += std::to_string(static_cast<uint32>(row.primitive));
            out += "|";
            out += std::to_string(static_cast<uint32>(row.surface_type));
            out += "|";
            out += std::to_string(static_cast<uint32>(row.geometry_mode));
            out += "|";
            out += std::to_string(static_cast<uint32>(row.position_provider));
            out += "|";
            out += GetVertexInputProfileName(row.vertex_input);
            out += "|";
            out += GetVertexTransformPolicyName(row.vertex_policy);
            out += "|";
            out += GetSurfaceShadingModelName(row.surface_model);
            out += "|";
            out += std::to_string(static_cast<uint32>(row.blend));
            out += "|";
            out += std::to_string(static_cast<uint32>(row.pass));
            out += "|";
            out += row.vs_template_path ? row.vs_template_path : "";
            out += "|";
            out += row.fs_template_path ? row.fs_template_path : "";
            out += "|";
            out += row.surface_path ? row.surface_path : "";
            out += "|";
            out += FormatShaderStageFeatureDescForLog(row.vs_features);
            out += "|";
            out += FormatShaderStageFeatureDescForLog(row.fs_features);
            out += "|";
            out += FormatMaterialResourcesForLog(row.resources);
            out += "|";
            out += std::to_string(static_cast<uint32>(row.schema));
            out += "|";
            out += GetStaticMaterialDefIdHintName(row.def_hint);
            out += "|";
            out += FormatRowTexturesForLog(row);
            out += "\n";
        }
    }

    return out;
}

void VariantRegistry::ForEachBuiltinRow(std::function<void(const MaterialVariantRow &)> cb) const
{
    for (const auto &row : builtin_row_storage)
        cb(row);
}

void VariantRegistry::ForEach(
    std::function<void(const MaterialVariantKey &, const MaterialVariantDesc &)> cb) const
{
    for (const auto &[hash, bucket] : variant_map)
        for (const auto &entry : bucket)
            cb(entry.key, entry.desc);
}

// ---------------------------------------------------------------------------
// Built-in variant table  (B' — C++20 designated-initializer flat table)
// Struct definition and BuildKey()/BuildDesc() live in BuiltinVariantEntry.h.
// ---------------------------------------------------------------------------

// Convenience aliases (TU-local; do not pollute hgl::graph::mtl public API).
namespace {
using ST   = _BVE_ST;
using GM   = _BVE_GM;
using TSM  = _BVE_TSM;
using LM   = _BVE_LM;
using RM   = _BVE_RM;
using PT   = _BVE_PT;
using Slot = _BVE_Slot;
constexpr auto VA = _BVE_VA;
} // anonymous (aliases only)

// clang-format off
const BuiltinVariantEntry kBuiltinVariants[] =
{
    // ── 2D ──────────────────────────────────────────────────────────────────────────────────────
    { .name = "VertexColor2D",      .preset = MaterialPreset::VertexColor2D,
      .geometry_mode = GM::Quad2D,  .position_provider = PositionProviderId::VAB_Vec2, .vertex_bits = VA(VertexAttrib::Color),
      .vs_path = "2d/vertexcolor2d.vert.glsl",  .fs_path = "2d/vertexcolor2d.frag.glsl"  },

    { .name = "PureColor2D",        .preset = MaterialPreset::PureColor2D,
      .geometry_mode = GM::Quad2D,  .position_provider = PositionProviderId::VAB_Vec2,
      .vs_path = "2d/purecolor2d.vert.glsl",    .fs_path = "2d/purecolor2d.frag.glsl"    },

    { .name = "PureTexture2D",      .preset = MaterialPreset::PureTexture2D,
      .geometry_mode = GM::Quad2D,  .position_provider = PositionProviderId::VAB_Vec2, .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "2d/puretexture2d.vert.glsl",  .fs_path = "2d/puretexture2d.frag.glsl"  },

    { .name = "PureTexture2DArray", .preset = MaterialPreset::PureTexture2D,
      .geometry_mode = GM::Quad2D,  .position_provider = PositionProviderId::VAB_Vec2, .tex = {{ Slot::BaseColor, TSM::Array }},
      .vs_path = "2d/puretexture2d.vert.glsl",  .fs_path = "2d/puretexture2d.frag.glsl"  },

    { .name = "Text2D",             .preset = MaterialPreset::Text2D,
      .geometry_mode = GM::Quad2D,  .position_provider = PositionProviderId::VAB_Vec2, .tex = {{ Slot::Text, TSM::Simple }},
      .vs_path = "2d/text2d.vert.glsl",         .fs_path = "2d/text2d.frag.glsl"         },

    // ── 3D Unlit ────────────────────────────────────────────────────────────────────────────────
    { .name = "PureColor3D",        .preset = MaterialPreset::PureColor3D },

    { .name = "VertexColor3D",      .preset = MaterialPreset::VertexColor3D,
      .vertex_bits = VA(VertexAttrib::Color),
      .surface_path = "surface/unlit_vertexcolor_surface.glsl" },

    { .name = "VertexLuminance3D",  .preset = MaterialPreset::VertexLuminance3D,
      .vertex_bits = VA(VertexAttrib::Luminance),
      .surface_path = "surface/unlit_luminance_surface.glsl"   },

    { .name = "VertexLuminance2D",  .preset = MaterialPreset::VertexLuminance2D,
      .position_provider = PositionProviderId::VAB_Vec2,
      .vertex_bits = VA(VertexAttrib::Luminance),
      .surface_path = "surface/unlit_luminance_surface.glsl"   },

    { .name = "VertexPaletteColor3D", .preset = MaterialPreset::VertexPaletteColor3D,
      .vertex_bits = VA(VertexAttrib::Color),
      .vs_path = "compositor/main_forward_unlit_palette.vert.glsl",
      .surface_path = "surface/unlit_vertexcolor_surface.glsl" },

    { .name = "Gizmo3D",            .preset = MaterialPreset::Gizmo3D,
      .surface_path = "surface/gizmo3d_surface.glsl" },

    // ── UnlitTexture3D (Mesh3D, BaseColor only, Simple + Array) ─────────────────────────────────
    { .name = "UnlitTexture3D",      .preset = MaterialPreset::UnlitTexture3D,
      .blend = RM::Transparent,     .pass = PT::ForwardTransparent,
      .vertex_bits = VA(VertexAttrib::TexCoord),
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .surface_path = "surface/unlit_texture3d_surface.glsl" },

    { .name = "UnlitTexture3DArray", .preset = MaterialPreset::UnlitTexture3D,
      .blend = RM::Transparent,     .pass = PT::ForwardTransparent,
      .vertex_bits = VA(VertexAttrib::TexCoord),
      .tex = {{ Slot::BaseColor, TSM::Array }},
      .surface_path = "surface/unlit_texture3d_surface.glsl" },

    // ── Billboard  (2 geometries × 5 blend modes) ───────────────────────────────────────────────
    { .name = "Billboard2DDynamicOpaque",   .preset = MaterialPreset::Billboard2DDynamic,
      .geometry_mode = GM::BillboardCameraFacing, .blend = RM::Opaque,          .pass = PT::ForwardOpaque,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_dynamic.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DDynamic",         .preset = MaterialPreset::Billboard2DDynamic,
      .geometry_mode = GM::BillboardCameraFacing, .blend = RM::Transparent,     .pass = PT::ForwardTransparent,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_dynamic.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DDynamicMasked",   .preset = MaterialPreset::Billboard2DDynamic,
      .geometry_mode = GM::BillboardCameraFacing, .blend = RM::Masked,          .pass = PT::ForwardMasked,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_dynamic.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DDynamicDither",   .preset = MaterialPreset::Billboard2DDynamic,
      .geometry_mode = GM::BillboardCameraFacing, .blend = RM::Dither,          .pass = PT::ForwardDither,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_dynamic.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DDynamicA2C",      .preset = MaterialPreset::Billboard2DDynamic,
      .geometry_mode = GM::BillboardCameraFacing, .blend = RM::AlphaToCoverage, .pass = PT::ForwardA2C,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_dynamic.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DFixedOpaque",     .preset = MaterialPreset::Billboard2DFixed,
      .geometry_mode = GM::BillboardAxisLocked,   .blend = RM::Opaque,          .pass = PT::ForwardOpaque,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_fixed.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DFixed",           .preset = MaterialPreset::Billboard2DFixed,
      .geometry_mode = GM::BillboardAxisLocked,   .blend = RM::Transparent,     .pass = PT::ForwardTransparent,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_fixed.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DFixedMasked",     .preset = MaterialPreset::Billboard2DFixed,
      .geometry_mode = GM::BillboardAxisLocked,   .blend = RM::Masked,          .pass = PT::ForwardMasked,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_fixed.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DFixedDither",     .preset = MaterialPreset::Billboard2DFixed,
      .geometry_mode = GM::BillboardAxisLocked,   .blend = RM::Dither,          .pass = PT::ForwardDither,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_fixed.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DFixedA2C",        .preset = MaterialPreset::Billboard2DFixed,
      .geometry_mode = GM::BillboardAxisLocked,   .blend = RM::AlphaToCoverage, .pass = PT::ForwardA2C,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_fixed.vert.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    // ── Terrain / Sky ────────────────────────────────────────────────────────────────────────────
    { .name = "TerrainGrid",  .preset = MaterialPreset::TerrainGrid,
      .surface_type = ST::Terrain,
      .vs_path = "compositor/main_terrain_grid.vert.glsl",
      .surface_path = "surface/terrain_grid_surface.glsl"  },

    { .name = "SkyMinimal",   .preset = MaterialPreset::SkyMinimal,
      .surface_type = ST::Sky,
      .surface_path = "surface/sky_minimal_surface.glsl"   },

    // ── Standard 3D Lit  (texture-based, BaseColor + Normal) ────────────────────────────────────
    { .name = "Standard",              .preset = MaterialPreset::Standard,
      .surface_type = ST::Standard, .lighting = LM::Lambert,
      .tex = {{ Slot::BaseColor, TSM::Simple }, { Slot::Normal, TSM::Simple }},
      .surface_path = "surface/standard_surface.glsl"           },

    { .name = "StandardBlinnPhong",    .preset = MaterialPreset::Standard,
      .surface_type = ST::Standard, .lighting = LM::BlinnPhong,
      .tex = {{ Slot::BaseColor, TSM::Simple }, { Slot::Normal, TSM::Simple }},
      .surface_path = "surface/textureblinnphong_surface.glsl"  },

    { .name = "StandardPBR",           .preset = MaterialPreset::Standard,
      .surface_type = ST::Standard, .lighting = LM::PBR,
      .tex = {{ Slot::BaseColor, TSM::Simple }, { Slot::Normal, TSM::Simple }},
      .surface_path = "surface/standard_surface.glsl"           },

    { .name = "StandardTextureArray",  .preset = MaterialPreset::Standard,
      .surface_type = ST::Standard, .lighting = LM::Lambert,
      .tex = {{ Slot::BaseColor, TSM::Array },  { Slot::Normal, TSM::Array  }},
      .surface_path = "surface/standard_surface.glsl"           },

    { .name = "StandardBlinnPhongArray", .preset = MaterialPreset::Standard,
      .surface_type = ST::Standard, .lighting = LM::BlinnPhong,
      .tex = {{ Slot::BaseColor, TSM::Array },  { Slot::Normal, TSM::Array  }},
      .surface_path = "surface/textureblinnphong_surface.glsl"  },

    { .name = "StandardPBRArray",      .preset = MaterialPreset::Standard,
      .surface_type = ST::Standard, .lighting = LM::PBR,
      .tex = {{ Slot::BaseColor, TSM::Array },  { Slot::Normal, TSM::Array  }},
      .surface_path = "surface/standard_surface.glsl"           },

    // ── PBR color-only ───────────────────────────────────────────────────────────────────────────
    { .name = "PBRColor3D",  .preset = MaterialPreset::PBRColor3D,
      .surface_type = ST::Standard, .lighting = LM::PBR,
      .surface_path = "surface/pbrcolor3d_surface.glsl" },

    // ── PCG / Fullscreen ─────────────────────────────────────────────────────────────────────────
    { .name = "FullscreenTriangle", .preset = MaterialPreset::FullscreenTriangle,
      .position_provider = PositionProviderId::PCG_FullscreenTriangle },
};
// clang-format on

const size_t kBuiltinVariantsCount = std::size(kBuiltinVariants);


void VariantRegistry::InitializeBuiltinVariants()
{
    builtin_row_storage.clear();
    builtin_row_storage.reserve(kBuiltinVariantsCount);

    for (const auto &e : kBuiltinVariants)
    {
        builtin_row_storage.emplace_back(BuildRowFromBuiltinVariantEntry(e));
        MaterialVariantDesc desc = BuildDesc(e);
        if (!builtin_row_storage.empty())
            desc.bound_row = &builtin_row_storage.back();
        RegisterVariant(BuildKey(e), desc);
    }
}

// ---------------------------------------------------------------------------
// Global singleton accessor — initialised on first call, then read-only
// ---------------------------------------------------------------------------
const VariantRegistry &GetBuiltinVariantRegistry()
{
    static VariantRegistry s_registry = []()
    {
        VariantRegistry r;
        r.InitializeBuiltinVariants();
        return r;
    }();
    return s_registry;
}

} // namespace hgl::graph::mtl
