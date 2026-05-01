#include<hgl/mtl/MaterialVariantRegistry.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include "BuiltinVariantEntry.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <initializer_list>
#include <string>

namespace hgl::graph::mtl{

namespace {

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
  text.reserve(320);

    text += "hash=";
    text += std::to_string(static_cast<unsigned long long>(key.Hash()));
    text += " ST=";
    text += std::to_string(static_cast<unsigned>(key.surface_type));
    text += " GM=";
    text += std::to_string(static_cast<unsigned>(key.geometry_mode));
    text += " PT=";
    text += std::to_string(static_cast<unsigned>(key.position_type));
    text += " blend=";
    text += std::to_string(static_cast<unsigned>(key.blend_mode));
    text += " pass=";
    text += std::to_string(static_cast<unsigned>(key.pass_hint));
    text += " sky=";
    text += std::to_string(static_cast<unsigned>(key.sky_ambient_model));
    text += " light=";
    text += std::to_string(static_cast<unsigned>(key.lighting_model));
    text += " eff=0x";

    char hex64[24] = {};
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

} // anonymous namespace

// ---------------------------------------------------------------------------
// VariantRegistry
// ---------------------------------------------------------------------------

void VariantRegistry::RegisterVariant(const MaterialVariantKey &key, const MaterialVariantDesc &desc)
{
    const uint64 hash = key.Hash();
    auto it = variant_map.find(hash);
    if (it != variant_map.end() && !(it->second.key == key))
    {
        std::fprintf(stderr,
            "[VariantRegistry] Hash collision on RegisterVariant '%s' (hash=0x%016llx); existing entry kept.\n",
            desc.variant_name.empty() ? "<unnamed>" : desc.variant_name.c_str(),
            static_cast<unsigned long long>(hash));
        return;
    }
    variant_map[hash] = VariantEntry{key, desc};
}

const MaterialVariantDesc *VariantRegistry::QueryVariant(const MaterialVariantKey &key,
                                                         const RegistryLookupOptions &options) const
{
    const MaterialVariantKey query_key = CanonicalizeRegistryLookupKey(key, options);

    auto it = variant_map.find(query_key.Hash());
    if (it == variant_map.end())
        return nullptr;
    if (!(it->second.key == query_key))
        return nullptr;
    return &it->second.desc;
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

    return nullptr;
}

bool VariantRegistry::ValidateBuiltinVariantTemplates(const std::string &shader_library_path,
                                                      std::vector<std::string> &diagnostics) const
{
    diagnostics.clear();

    CompositorAssembler assembler(shader_library_path);

    for(const auto &[hash,entry]:variant_map)
    {
        const auto result=assembler.Assemble(entry.key,entry.desc);
        if(result.success)
            continue;

        std::string msg="Variant validation failed: ";
        msg += entry.desc.variant_name.empty()?"<unnamed>":entry.desc.variant_name;
        msg += " (hash=";
        msg += std::to_string(hash);
        msg += ")";

        if(!result.error_message.empty())
        {
            msg += " - ";
            msg += result.error_message;
        }

        diagnostics.emplace_back(std::move(msg));
    }

    return diagnostics.empty();
}

std::string VariantRegistry::DumpSnapshot() const
{
    std::vector<std::pair<uint64, const VariantEntry *>> rows;
    rows.reserve(variant_map.size());

    for(const auto &[hash,entry]:variant_map)
        rows.emplace_back(hash,&entry);

    std::sort(rows.begin(),rows.end(),
        [](const auto &a,const auto &b)
        {
            return a.first<b.first;
        });

    std::string out;
    out.reserve(rows.size()*120);

    out += "# VariantRegistry Snapshot\n";
    out += "hash|name|factory|surface|geometry|tex_modes|tex_bits|sampler_bits|va_bits|extra_bits|blend|pass\n";

    for(const auto &[hash,entry_ptr]:rows)
    {
        const auto &entry=*entry_ptr;
        const auto &k=entry.key;
        const auto &d=entry.desc;

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
        out += "\n";
    }

    return out;
}

MaterialVariantKey VariantRegistry::MapPresetToVariantKey(MaterialPreset preset) const
{
    // [Step 3.5 T1] Delegate to RouteKey (single source of truth) instead of the
    // deprecated free function MapPresetToVariantKey.
    return hgl::graph::mtl::RouteKey(preset);
}

void VariantRegistry::ForEach(
    std::function<void(const MaterialVariantKey &, const MaterialVariantDesc &)> cb) const
{
    for (const auto &[hash, entry] : variant_map)
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
constexpr auto EX = _BVE_EX;
} // anonymous (aliases only)

// clang-format off
const BuiltinVariantEntry kBuiltinVariants[] =
{
    // ── 2D ──────────────────────────────────────────────────────────────────────────────────────
    { .name = "VertexColor2D",      .preset = MaterialPreset::VertexColor2D,
      .geometry_mode = GM::Quad2D,  .position_type = PositionType::Vec2, .position_provider = PositionProviderId::VAB_Vec2, .vertex_bits = VA(VertexAttrib::Color),
      .vs_path = "2d/vertexcolor2d.vert.glsl",  .fs_path = "2d/vertexcolor2d.frag.glsl"  },

    { .name = "PureColor2D",        .preset = MaterialPreset::PureColor2D,
      .geometry_mode = GM::Quad2D,  .position_type = PositionType::Vec2, .position_provider = PositionProviderId::VAB_Vec2,
      .vs_path = "2d/purecolor2d.vert.glsl",    .fs_path = "2d/purecolor2d.frag.glsl"    },

    { .name = "PureTexture2D",      .preset = MaterialPreset::PureTexture2D,
      .geometry_mode = GM::Quad2D,  .position_type = PositionType::Vec2, .position_provider = PositionProviderId::VAB_Vec2, .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "2d/puretexture2d.vert.glsl",  .fs_path = "2d/puretexture2d.frag.glsl"  },

    { .name = "PureTexture2DArray", .preset = MaterialPreset::PureTexture2D,
      .geometry_mode = GM::Quad2D,  .position_type = PositionType::Vec2, .position_provider = PositionProviderId::VAB_Vec2, .tex = {{ Slot::BaseColor, TSM::Array }},
      .vs_path = "2d/puretexture2d.vert.glsl",  .fs_path = "2d/puretexture2d.frag.glsl"  },

    { .name = "Text2D",             .preset = MaterialPreset::Text2D,
      .geometry_mode = GM::Quad2D,  .position_type = PositionType::Vec2, .position_provider = PositionProviderId::VAB_Vec2, .tex = {{ Slot::BaseColor, TSM::Atlas }},
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
      .position_type = PositionType::Vec2, .position_provider = PositionProviderId::VAB_Vec2,
      .vertex_bits = VA(VertexAttrib::Luminance),
      .surface_path = "surface/unlit_luminance_surface.glsl"   },

    { .name = "VertexPaletteColor3D", .preset = MaterialPreset::VertexPaletteColor3D,
      .vertex_bits = VA(VertexAttrib::Color), .extra_bits = EX(ExtraFeature::DebugShading),
      .vs_path = "compositor/main_forward_unlit_palette.vert.glsl",
      .surface_path = "surface/unlit_vertexcolor_surface.glsl" },

    { .name = "Gizmo3D",            .preset = MaterialPreset::Gizmo3D,
      .extra_bits = EX(ExtraFeature::DebugShading),
      .surface_path = "surface/gizmo3d_surface.glsl" },

    // ── Billboard  (2 geometries × 5 blend modes) ───────────────────────────────────────────────
    { .name = "Billboard2DDynamicOpaque",   .preset = MaterialPreset::Billboard2DDynamic,
      .geometry_mode = GM::BillboardCameraFacing, .blend = RM::Opaque,          .pass = PT::ForwardOpaque,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_dynamic.vert.glsl",
      .fs_path = "compositor/main_forward_billboard.frag.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DDynamic",         .preset = MaterialPreset::Billboard2DDynamic,
      .geometry_mode = GM::BillboardCameraFacing, .blend = RM::Transparent,     .pass = PT::ForwardTransparent,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_dynamic.vert.glsl",
      .fs_path = "compositor/main_forward_billboard.frag.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DDynamicMasked",   .preset = MaterialPreset::Billboard2DDynamic,
      .geometry_mode = GM::BillboardCameraFacing, .blend = RM::Masked,          .pass = PT::ForwardMasked,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_dynamic.vert.glsl",
      .fs_path = "compositor/main_forward_billboard.frag.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DDynamicDither",   .preset = MaterialPreset::Billboard2DDynamic,
      .geometry_mode = GM::BillboardCameraFacing, .blend = RM::Dither,          .pass = PT::ForwardDither,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_dynamic.vert.glsl",
      .fs_path = "compositor/main_forward_billboard.frag.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DDynamicA2C",      .preset = MaterialPreset::Billboard2DDynamic,
      .geometry_mode = GM::BillboardCameraFacing, .blend = RM::AlphaToCoverage, .pass = PT::ForwardA2C,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_dynamic.vert.glsl",
      .fs_path = "compositor/main_forward_billboard.frag.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DFixedOpaque",     .preset = MaterialPreset::Billboard2DFixed,
      .geometry_mode = GM::BillboardAxisLocked,   .blend = RM::Opaque,          .pass = PT::ForwardOpaque,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_fixed.vert.glsl",
      .fs_path = "compositor/main_forward_billboard.frag.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DFixed",           .preset = MaterialPreset::Billboard2DFixed,
      .geometry_mode = GM::BillboardAxisLocked,   .blend = RM::Transparent,     .pass = PT::ForwardTransparent,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_fixed.vert.glsl",
      .fs_path = "compositor/main_forward_billboard.frag.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DFixedMasked",     .preset = MaterialPreset::Billboard2DFixed,
      .geometry_mode = GM::BillboardAxisLocked,   .blend = RM::Masked,          .pass = PT::ForwardMasked,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_fixed.vert.glsl",
      .fs_path = "compositor/main_forward_billboard.frag.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DFixedDither",     .preset = MaterialPreset::Billboard2DFixed,
      .geometry_mode = GM::BillboardAxisLocked,   .blend = RM::Dither,          .pass = PT::ForwardDither,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_fixed.vert.glsl",
      .fs_path = "compositor/main_forward_billboard.frag.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    { .name = "Billboard2DFixedA2C",        .preset = MaterialPreset::Billboard2DFixed,
      .geometry_mode = GM::BillboardAxisLocked,   .blend = RM::AlphaToCoverage, .pass = PT::ForwardA2C,
      .tex = {{ Slot::BaseColor, TSM::Simple }},
      .vs_path = "compositor/main_forward_billboard_fixed.vert.glsl",
      .fs_path = "compositor/main_forward_billboard.frag.glsl",
      .surface_path = "surface/billboard_texture_surface.glsl" },

    // ── Terrain / Sky ────────────────────────────────────────────────────────────────────────────
    { .name = "TerrainGrid",  .preset = MaterialPreset::TerrainGrid,
      .surface_type = ST::Terrain,
      .vs_path = "compositor/main_terrain_grid.vert.glsl",
      .surface_path = "surface/terrain_grid_surface.glsl"  },

    { .name = "SkyMinimal",   .preset = MaterialPreset::SkyMinimal,
      .surface_type = ST::Sky,
      .vs_path = "compositor/main_forward_sky.vert.glsl",
      .fs_path = "compositor/main_forward_sky.frag.glsl",
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
      .position_type = PositionType::None, .position_provider = PositionProviderId::PCG_FullscreenTriangle },
};
// clang-format on

const size_t kBuiltinVariantsCount = std::size(kBuiltinVariants);

void VariantRegistry::InitializeBuiltinVariants()
{
    for (const auto &e : kBuiltinVariants)
        RegisterVariant(BuildKey(e), BuildDesc(e));
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
