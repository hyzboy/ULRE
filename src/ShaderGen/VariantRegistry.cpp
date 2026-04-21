#include<hgl/mtl/MaterialVariantRegistry.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/shadergen/CompositorAssembler.h>
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

#if defined(ULRE_SHADERGEN_REGISTRY_MATCH_EFFECTIVE_MASK)
constexpr bool kRegistryMatchEffectiveFeatureMask = true;
#else
constexpr bool kRegistryMatchEffectiveFeatureMask = false;
#endif

struct VariantRegistryFallbackStats
{
    std::atomic<unsigned long long> exact{0};
    std::atomic<unsigned long long> miss{0};
};

static VariantRegistryFallbackStats g_variant_registry_stats;

static MaterialVariantKey CanonicalizeRegistryLookupKey(const MaterialVariantKey &key)
{
    MaterialVariantKey canon = key;

    if (!kRegistryMatchEffectiveFeatureMask && canon.effective_feature_mask != 0)
    {
        static std::atomic_bool s_warned_effective_mask_ignored{false};
        bool expected = false;
        if (s_warned_effective_mask_ignored.compare_exchange_strong(expected, true, std::memory_order_relaxed))
        {
            std::fprintf(stderr,
                "[VariantRegistry] warning: effective_feature_mask is ignored for variant routing by default; "
                "define ULRE_SHADERGEN_REGISTRY_MATCH_EFFECTIVE_MASK to include it in registry lookup.\n");
        }

        canon.effective_feature_mask = 0;
    }

    return canon;
}

static unsigned long long RecordAndGetTotalQueries(std::atomic<unsigned long long> &bucket)
{
    bucket.fetch_add(1, std::memory_order_relaxed);

    const auto exact = g_variant_registry_stats.exact.load(std::memory_order_relaxed);
    const auto miss = g_variant_registry_stats.miss.load(std::memory_order_relaxed);
    return exact + miss;
}

static void MaybePrintStatsSummary(const char *reason, const unsigned long long total)
{
    if (!kVariantRegistryVerbose)
        return;

    if (total < 64)
        return;

    if ((total % 128) != 0 && std::string(reason) != "miss")
        return;

    std::fprintf(stderr,
        "[VariantRegistry] stats reason=%s total=%llu exact=%llu miss=%llu\n",
        reason ? reason : "unknown",
        total,
        g_variant_registry_stats.exact.load(std::memory_order_relaxed),
        g_variant_registry_stats.miss.load(std::memory_order_relaxed));
}

static std::string FormatVariantKeyForLog(const MaterialVariantKey &key)
{
    std::string text;
    text.reserve(256);

    text += "hash=";
    text += std::to_string(static_cast<unsigned long long>(key.Hash()));
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

const MaterialVariantDesc *VariantRegistry::QueryVariant(const MaterialVariantKey &key) const
{
    const MaterialVariantKey query_key = CanonicalizeRegistryLookupKey(key);

    auto it = variant_map.find(query_key.Hash());
    if (it == variant_map.end())
        return nullptr;
    if (!(it->second.key == query_key))
        return nullptr;
    return &it->second.desc;
}

const MaterialVariantDesc *VariantRegistry::QueryVariantWithCanonicalFallback(
    const MaterialVariantKey &key,
    MaterialVariantKey *resolved_key) const
{
    const MaterialVariantKey request_key = CanonicalizeRegistryLookupKey(key);

    if(const MaterialVariantDesc *exact=QueryVariant(request_key))
    {
        const auto total = RecordAndGetTotalQueries(g_variant_registry_stats.exact);
        if (kVariantRegistryVerbose)
        {
            std::fprintf(stderr,
                "[VariantRegistry] exact-match variant=%s %s\n",
                exact->variant_name.empty() ? "<unnamed>" : exact->variant_name.c_str(),
                FormatVariantKeyForLog(request_key).c_str());
        }
        MaybePrintStatsSummary("exact", total);
        if(resolved_key)
            *resolved_key=request_key;
        return exact;
    }

    const auto total = RecordAndGetTotalQueries(g_variant_registry_stats.miss);
    if (kVariantRegistryVerbose)
    {
        std::fprintf(stderr,
            "[VariantRegistry] miss request={%s}\n",
            FormatVariantKeyForLog(request_key).c_str());
    }
    MaybePrintStatsSummary("miss", total);

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
    // Delegate to the free function in MaterialLibrary.cpp
    return hgl::graph::mtl::MapPresetToVariantKey(preset);
}

// ---------------------------------------------------------------------------
// Built-in variant registrations
// Each entry mirrors the shader paths used in the corresponding M_*.cpp file.
// ---------------------------------------------------------------------------

namespace {

// Helper: build a desc with explicit shader paths
MaterialVariantDesc MakeDesc(
    const char *name,
    const MaterialPreset factory_type,
    const char *vs_path,
    const char *fs_path,
    const char *surface_path = nullptr)
{
    MaterialVariantDesc d;
    d.variant_name          = name;
    d.factory_type          = factory_type;
    d.vs_template_path      = vs_path;
    d.fs_template_path      = fs_path;
    d.surface_function_path = surface_path ? surface_path : "";
    return d;
}

// Helper: build a key like MapPresetToVariantKey
inline MaterialVariantKey K(SurfaceType st,
                             GeometryMode gm,
                             std::initializer_list<std::pair<SamplerSlot, TextureSourceMode>> tex_modes = {},
                             uint32 vertex_bits = 0,
                             uint32 sampler_bits = 0,
                             uint32 extra_bits = static_cast<uint32>(ExtraFeature::None))
{
    MaterialVariantKey k;
    k.surface_type        = st;
    k.geometry_mode       = gm;
    k.sampler_feature_bits = sampler_bits;
    for(const auto& pair : tex_modes)
    {
        k.SetTextureSourceMode(pair.first, pair.second);
    }
    k.vertex_attribute_feature_bits = vertex_bits;
    k.extra_feature_bits = extra_bits;
    return k;
}

// Helper: build a billboard key (BaseColor/Simple, has-texture, explicit blend+pass).
inline MaterialVariantKey KB(GeometryMode gm, RenderAlphaMode blend, PassType pass)
{
    MaterialVariantKey k;
    k.geometry_mode = gm;
    k.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Simple);
    k.blend_mode = blend;
    k.pass_hint  = pass;
    return k;
}

} // anonymous namespace

void VariantRegistry::InitializeBuiltinVariants()
{
    using ST  = SurfaceType;
    using GM  = GeometryMode;
    using TSM = TextureSourceMode;

    RegisterVariant(
        K(ST::Unlit, GM::Quad2D, {},
            VertexAttribFeatureBit(VertexAttrib::Color)),
        MakeDesc("VertexColor2D",
                 MaterialPreset::VertexColor2D,
                 "2d/vertexcolor2d.vert.glsl",
                 "2d/vertexcolor2d.frag.glsl",
                 ""));

    RegisterVariant(
        K(ST::Unlit, GM::Quad2D),
        MakeDesc("PureColor2D",
                 MaterialPreset::PureColor2D,
                 "2d/purecolor2d.vert.glsl",
                 "2d/purecolor2d.frag.glsl",
                 ""));

    RegisterVariant(
        K(ST::Unlit, GM::Quad2D, {{SamplerSlot::BaseColor, TSM::Simple}},
            0,
            SamplerFeatureBit(SamplerSlot::BaseColor)),
        MakeDesc("PureTexture2D",
                 MaterialPreset::PureTexture2D,
                 "2d/puretexture2d.vert.glsl",
                 "2d/puretexture2d.frag.glsl",
                 ""));

    RegisterVariant(
        K(ST::Unlit, GM::Quad2D, {{SamplerSlot::BaseColor, TSM::Array}},
            0,
            SamplerFeatureBit(SamplerSlot::BaseColor)),
        MakeDesc("PureTexture2DArray",
                 MaterialPreset::PureTexture2D,
                 "2d/puretexture2d.vert.glsl",
                 "2d/puretexture2d.frag.glsl",
                 ""));

    RegisterVariant(
        K(ST::Unlit, GM::Quad2D, {{SamplerSlot::BaseColor, TSM::Atlas}},
            0,
            SamplerFeatureBit(SamplerSlot::BaseColor)),
        MakeDesc("Text2D",
                 MaterialPreset::Text2D,
                 "2d/text2d.vert.glsl",
                 "2d/text2d.frag.glsl",
                 ""));

    // ------------------------------------------------------------------
    // 3D Unlit: PureColor — uses CompositorAssembler auto-routing, no explicit paths
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Unlit, GM::Mesh3D),
        MakeDesc("PureColor3D", MaterialPreset::PureColor3D, "", "", ""));

    // ------------------------------------------------------------------
    // 3D Unlit: VertexColor
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Unlit, GM::Mesh3D, {},
            VertexAttribFeatureBit(VertexAttrib::Color)),
        MakeDesc("VertexColor3D",
                 MaterialPreset::VertexColor3D,
                 "compositor/main_forward_unlit_vertexcolor.vert.glsl",
                 "compositor/main_forward_unlit_vertexcolor.frag.glsl",
                 "surface/unlit_vertexcolor_surface.glsl"));

    // ------------------------------------------------------------------
    // 3D Unlit: VertexLuminance3D (VEC3 position)
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Unlit, GM::Mesh3D, {},
            VertexAttribFeatureBit(VertexAttrib::Luminance)),
        MakeDesc("VertexLuminance3D",
                 MaterialPreset::VertexLuminance3D,
                 "compositor/main_forward_unlit_luminance.vert.glsl",
                 "compositor/main_forward_unlit_luminance.frag.glsl",
                 "surface/unlit_luminance_surface.glsl"));

    // ------------------------------------------------------------------
    // 3D Unlit: VertexLuminance2D (VEC2 position)
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Unlit, GM::Mesh3D, {},
            VertexAttribFeatureBit(VertexAttrib::Luminance) | VertexAttribFeatureBit(VertexAttrib::Position)),
        MakeDesc("VertexLuminance2D",
                 MaterialPreset::VertexLuminance2D,
                 "compositor/main_forward_unlit_luminance_2d.vert.glsl",
                 "compositor/main_forward_unlit_luminance.frag.glsl",
                 "surface/unlit_luminance_surface.glsl"));

    // ------------------------------------------------------------------
    // 3D Unlit: VertexPaletteColor
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Unlit, GM::Mesh3D, {},
            VertexAttribFeatureBit(VertexAttrib::Color),
            0,
            static_cast<uint32>(ExtraFeature::DebugShading)),
        MakeDesc("VertexPaletteColor3D",
                 MaterialPreset::VertexPaletteColor3D,
                 "compositor/main_forward_unlit_palette.vert.glsl",
                 "compositor/main_forward_unlit_vertexcolor.frag.glsl",
                 "surface/unlit_vertexcolor_surface.glsl"));

    // ------------------------------------------------------------------
    // 3D Unlit: Gizmo
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Unlit, GM::Mesh3D, {}, 0, 0, static_cast<uint32>(ExtraFeature::DebugShading)),
        MakeDesc("Gizmo3D",
                 MaterialPreset::Gizmo3D,
                 "compositor/main_forward_unlit_normal.vert.glsl",
                 "compositor/main_forward_unlit_normal.frag.glsl",
                 "surface/gizmo3d_surface.glsl"));

    // ------------------------------------------------------------------
    // Billboard variants (2 geometries × 4 blend modes)
    // ------------------------------------------------------------------
    static const struct {
        const char    *name;
        MaterialPreset preset;
        GeometryMode   gm;
        RenderAlphaMode      blend;
        PassType       pass;
        const char    *vs_path;
    } kBillboardVariants[] = {
        {"Billboard2DDynamicOpaque", MaterialPreset::Billboard2DDynamic, GM::BillboardCameraFacing, RenderAlphaMode::Opaque,           PassType::ForwardOpaque,      "compositor/main_forward_billboard_dynamic.vert.glsl"},
        {"Billboard2DDynamic",       MaterialPreset::Billboard2DDynamic, GM::BillboardCameraFacing, RenderAlphaMode::Transparent,      PassType::ForwardTransparent, "compositor/main_forward_billboard_dynamic.vert.glsl"},
        {"Billboard2DDynamicMasked", MaterialPreset::Billboard2DDynamic, GM::BillboardCameraFacing, RenderAlphaMode::Masked,           PassType::ForwardMasked,      "compositor/main_forward_billboard_dynamic.vert.glsl"},
        {"Billboard2DDynamicDither", MaterialPreset::Billboard2DDynamic, GM::BillboardCameraFacing, RenderAlphaMode::Dither,           PassType::ForwardDither,      "compositor/main_forward_billboard_dynamic.vert.glsl"},
        {"Billboard2DDynamicA2C",    MaterialPreset::Billboard2DDynamic, GM::BillboardCameraFacing, RenderAlphaMode::AlphaToCoverage,  PassType::ForwardA2C,         "compositor/main_forward_billboard_dynamic.vert.glsl"},
        {"Billboard2DFixedOpaque",   MaterialPreset::Billboard2DFixed,   GM::BillboardAxisLocked,   RenderAlphaMode::Opaque,           PassType::ForwardOpaque,      "compositor/main_forward_billboard_fixed.vert.glsl"},
        {"Billboard2DFixed",         MaterialPreset::Billboard2DFixed,   GM::BillboardAxisLocked,   RenderAlphaMode::Transparent,      PassType::ForwardTransparent, "compositor/main_forward_billboard_fixed.vert.glsl"},
        {"Billboard2DFixedMasked",   MaterialPreset::Billboard2DFixed,   GM::BillboardAxisLocked,   RenderAlphaMode::Masked,           PassType::ForwardMasked,      "compositor/main_forward_billboard_fixed.vert.glsl"},
        {"Billboard2DFixedDither",   MaterialPreset::Billboard2DFixed,   GM::BillboardAxisLocked,   RenderAlphaMode::Dither,           PassType::ForwardDither,      "compositor/main_forward_billboard_fixed.vert.glsl"},
        {"Billboard2DFixedA2C",      MaterialPreset::Billboard2DFixed,   GM::BillboardAxisLocked,   RenderAlphaMode::AlphaToCoverage,  PassType::ForwardA2C,         "compositor/main_forward_billboard_fixed.vert.glsl"},
    };
    for (const auto &e : kBillboardVariants)
        RegisterVariant(KB(e.gm, e.blend, e.pass),
            MakeDesc(e.name, e.preset,
                     e.vs_path,
                     "compositor/main_forward_billboard.frag.glsl",
                     "surface/billboard_texture_surface.glsl"));

    // ------------------------------------------------------------------
    // Terrain
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Terrain, GM::Mesh3D),
        MakeDesc("TerrainGrid",
                 MaterialPreset::TerrainGrid,
                 "compositor/main_terrain_grid.vert.glsl",
                 "compositor/main_terrain_grid.frag.glsl",
                 "surface/terrain_grid_surface.glsl"));

    // ------------------------------------------------------------------
    // Sky
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Sky, GM::Mesh3D),
        MakeDesc("SkyMinimal",
                 MaterialPreset::SkyMinimal,
                 "compositor/main_forward_sky.vert.glsl",
                 "compositor/main_forward_sky.frag.glsl",
                 "surface/sky_minimal_surface.glsl"));

    // ------------------------------------------------------------------
    // Standard 3D variants (texture-based, lit)
    // ------------------------------------------------------------------
    static const struct {
        const char *name;
        TSM tsm;
        LightingModel lighting;
        const char *surface_path;
    } kStdVariants[] = {
        {"Standard",                 TSM::Simple, LightingModel::Lambert,    "surface/standard_surface.glsl"},
        {"StandardBlinnPhong",       TSM::Simple, LightingModel::BlinnPhong, "surface/textureblinnphong_surface.glsl"},
        {"StandardPBR",              TSM::Simple, LightingModel::PBR,        "surface/standard_surface.glsl"},
        {"StandardTextureArray",     TSM::Array,  LightingModel::Lambert,    "surface/standard_surface.glsl"},
        {"StandardBlinnPhongArray",  TSM::Array,  LightingModel::BlinnPhong, "surface/textureblinnphong_surface.glsl"},
        {"StandardPBRArray",         TSM::Array,  LightingModel::PBR,        "surface/standard_surface.glsl"},
    };
    for (const auto &e : kStdVariants)
    {
        MaterialVariantKey key = K(ST::Standard, GM::Mesh3D,
            {{SamplerSlot::BaseColor, e.tsm}, {SamplerSlot::Normal, e.tsm}},
            0,
            SamplerFeatureBit(SamplerSlot::BaseColor) | SamplerFeatureBit(SamplerSlot::Normal));
        key.lighting_model = e.lighting;

        RegisterVariant(
            key,
            MakeDesc(e.name,
                     MaterialPreset::Standard,
                     "compositor/main_forward_lit.vert.glsl",
                     "compositor/main_forward_lit.frag.glsl",
                     e.surface_path));
    }

    // ------------------------------------------------------------------
    // PBRColor3D (Standard surface, color-only via MaterialBindingInstanceData)
    // ------------------------------------------------------------------
    {
        MaterialVariantKey key = K(ST::Standard, GM::Mesh3D);
        key.lighting_model = LightingModel::PBR;

        RegisterVariant(
            key,
            MakeDesc("PBRColor3D",
                     MaterialPreset::PBRColor3D,
                     "compositor/main_forward_lit.vert.glsl",
                     "compositor/main_forward_lit.frag.glsl",
                     "surface/pbrcolor3d_surface.glsl"));
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
