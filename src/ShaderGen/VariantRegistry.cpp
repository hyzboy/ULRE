#include<hgl/mtl/MaterialVariantRegistry.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include <algorithm>
#include <initializer_list>

namespace hgl::graph::mtl{

// ---------------------------------------------------------------------------
// VariantRegistry
// ---------------------------------------------------------------------------

void VariantRegistry::RegisterVariant(const MaterialVariantKey &key, const MaterialVariantDesc &desc)
{
    variant_map[key.Hash()] = VariantEntry{key,desc};
}

const MaterialVariantDesc *VariantRegistry::QueryVariant(const MaterialVariantKey &key) const
{
    auto it = variant_map.find(key.Hash());
    if (it == variant_map.end())
        return nullptr;
    return &it->second.desc;
}

const MaterialVariantDesc *VariantRegistry::QueryVariantWithCanonicalFallback(
    const MaterialVariantKey &key,
    MaterialVariantKey *resolved_key) const
{
    if(const MaterialVariantDesc *exact=QueryVariant(key))
    {
        if(resolved_key)
            *resolved_key=key;
        return exact;
    }

    // Fallback step 1: ignore per-slot source bits but preserve sampler feature bits.
    // This matches legacy registry keys that encode coarse mode + sampler mask only.
    MaterialVariantKey canon=key;
    canon.texture_source_bits=0;

    if(const MaterialVariantDesc *fallback=QueryVariant(canon))
    {
        if(resolved_key)
            *resolved_key=canon;
        return fallback;
    }

    // Fallback step 2: fully canonicalized (legacy coarse key).
    canon.sampler_feature_bits=0;

    if(const MaterialVariantDesc *fallback=QueryVariant(canon))
    {
        if(resolved_key)
            *resolved_key=canon;
        return fallback;
    }

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
        out += d.has_factory_type
            ? std::to_string(static_cast<uint32>(d.factory_type))
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
    d.has_factory_type      = true;
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
inline MaterialVariantKey KB(GeometryMode gm, BlendMode blend, PassType pass)
{
    MaterialVariantKey k;
    k.geometry_mode = gm;
    k.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Simple);
    k.SetHasTexture(SamplerSlot::BaseColor);
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
    // 3D Unlit: VertexPattleColor
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Unlit, GM::Mesh3D, {},
            VertexAttribFeatureBit(VertexAttrib::Color),
            0,
            static_cast<uint32>(ExtraFeature::DebugShading)),
        MakeDesc("VertexPattleColor3D",
                 MaterialPreset::VertexPattleColor3D,
                 "compositor/main_forward_unlit_pattle.vert.glsl",
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
    // Billboard variants (2 geometries × 3 blend modes)
    // ------------------------------------------------------------------
    static const struct {
        const char    *name;
        MaterialPreset preset;
        GeometryMode   gm;
        BlendMode      blend;
        PassType       pass;
        const char    *vs_path;
    } kBillboardVariants[] = {
        {"Billboard2DDynamic",       MaterialPreset::Billboard2DDynamic, GM::BillboardCameraFacing, BlendMode::Transparent,      PassType::ForwardTransparent, "compositor/main_forward_billboard_dynamic.vert.glsl"},
        {"Billboard2DDynamicMasked", MaterialPreset::Billboard2DDynamic, GM::BillboardCameraFacing, BlendMode::Masked,           PassType::ForwardMasked,      "compositor/main_forward_billboard_dynamic.vert.glsl"},
        {"Billboard2DDynamicDither", MaterialPreset::Billboard2DDynamic, GM::BillboardCameraFacing, BlendMode::Dither,           PassType::ForwardDither,      "compositor/main_forward_billboard_dynamic.vert.glsl"},
        {"Billboard2DDynamicA2C",    MaterialPreset::Billboard2DDynamic, GM::BillboardCameraFacing, BlendMode::AlphaToCoverage,  PassType::ForwardA2C,         "compositor/main_forward_billboard_dynamic.vert.glsl"},
        {"Billboard2DFixed",         MaterialPreset::Billboard2DFixed,   GM::BillboardAxisLocked,   BlendMode::Transparent,      PassType::ForwardTransparent, "compositor/main_forward_billboard_fixed.vert.glsl"},
        {"Billboard2DFixedMasked",   MaterialPreset::Billboard2DFixed,   GM::BillboardAxisLocked,   BlendMode::Masked,           PassType::ForwardMasked,      "compositor/main_forward_billboard_fixed.vert.glsl"},
        {"Billboard2DFixedDither",   MaterialPreset::Billboard2DFixed,   GM::BillboardAxisLocked,   BlendMode::Dither,           PassType::ForwardDither,      "compositor/main_forward_billboard_fixed.vert.glsl"},
        {"Billboard2DFixedA2C",      MaterialPreset::Billboard2DFixed,   GM::BillboardAxisLocked,   BlendMode::AlphaToCoverage,  PassType::ForwardA2C,         "compositor/main_forward_billboard_fixed.vert.glsl"},
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
    static const struct { const char *name; TSM tsm; } kStdVariants[] = {
        {"Standard",             TSM::Simple},
        {"StandardTextureArray", TSM::Array},
    };
    for (const auto &e : kStdVariants)
        RegisterVariant(
            K(ST::Standard, GM::Mesh3D,
              {{SamplerSlot::BaseColor, e.tsm}, {SamplerSlot::Normal, e.tsm}},
              0,
              SamplerFeatureBit(SamplerSlot::BaseColor) | SamplerFeatureBit(SamplerSlot::Normal)),
            MakeDesc(e.name,
                     MaterialPreset::Standard,
                     "compositor/main_forward_lit.vert.glsl",
                     "compositor/main_forward_lit.frag.glsl",
                     "surface/standard_surface.glsl"));

    // ------------------------------------------------------------------
    // PBRColor3D (Standard surface, color-only via MaterialInstanceData)
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Standard, GM::Mesh3D),
        MakeDesc("PBRColor3D",
                 MaterialPreset::PBRColor3D,
                 "compositor/main_forward_lit.vert.glsl",
                 "compositor/main_forward_lit.frag.glsl",
                 "surface/pbrcolor3d_surface.glsl"));
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
