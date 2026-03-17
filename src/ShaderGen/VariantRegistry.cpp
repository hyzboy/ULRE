#include<hgl/mtl/MaterialVariantDesc.h>
#include<hgl/mtl/MaterialLibrary.h>

namespace hgl::graph::mtl{

// ---------------------------------------------------------------------------
// VariantRegistry
// ---------------------------------------------------------------------------

void VariantRegistry::RegisterVariant(const MaterialVariantKey &key, const MaterialVariantDesc &desc)
{
    variant_map[key.Hash()] = desc;
}

const MaterialVariantDesc *VariantRegistry::QueryVariant(const MaterialVariantKey &key) const
{
    auto it = variant_map.find(key.Hash());
    if (it == variant_map.end())
        return nullptr;
    return &it->second;
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
    const char *vs_path,
    const char *fs_path,
    const char *surface_path = nullptr)
{
    MaterialVariantDesc d;
    d.variant_name          = name;
    d.vs_template_path      = vs_path;
    d.fs_template_path      = fs_path;
    d.surface_function_path = surface_path ? surface_path : "";
    return d;
}

// Helper: build a key like MapPresetToVariantKey
inline MaterialVariantKey K(SurfaceType st,
                             GeometryMode gm,
                             TextureSourceMode tsm = TextureSourceMode::None,
                             uint32 fb = VF_None)
{
    MaterialVariantKey k;
    k.surface_type        = st;
    k.geometry_mode       = gm;
    k.texture_source_mode = tsm;
    k.feature_bits        = fb;
    return k;
}

} // anonymous namespace

void VariantRegistry::InitializeBuiltinVariants()
{
    using ST  = SurfaceType;
    using GM  = GeometryMode;
    using TSM = TextureSourceMode;

    // ------------------------------------------------------------------
    // 3D Unlit: PureColor — uses CompositorAssembler auto-routing, no explicit paths
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Unlit, GM::Mesh3D),
        MakeDesc("PureColor3D", "", "", ""));

    // ------------------------------------------------------------------
    // 3D Unlit: VertexColor
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Unlit, GM::Mesh3D, TSM::None, VF_UseVertexColor),
        MakeDesc("VertexColor3D",
                 "compositor/main_forward_unlit_vertexcolor.vert.glsl",
                 "compositor/main_forward_unlit_vertexcolor.frag.glsl",
                 "surface/unlit_vertexcolor_surface.glsl"));

    // ------------------------------------------------------------------
    // 3D Unlit: VertexLuminance3D (VEC3 position)
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Unlit, GM::Mesh3D, TSM::None, VF_UseVertexLum),
        MakeDesc("VertexLuminance3D",
                 "compositor/main_forward_unlit_luminance.vert.glsl",
                 "compositor/main_forward_unlit_luminance.frag.glsl",
                 "surface/unlit_luminance_surface.glsl"));

    // ------------------------------------------------------------------
    // 3D Unlit: VertexLuminance2D (VEC2 position)
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Unlit, GM::Mesh3D, TSM::None, VF_UseVertexLum | VF_UsePos2D),
        MakeDesc("VertexLuminance2D",
                 "compositor/main_forward_unlit_luminance_2d.vert.glsl",
                 "compositor/main_forward_unlit_luminance.frag.glsl",
                 "surface/unlit_luminance_surface.glsl"));

    // ------------------------------------------------------------------
    // 3D Unlit: VertexPattleColor
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Unlit, GM::Mesh3D, TSM::None, VF_UseVertexColor | VF_DebugShading),
        MakeDesc("VertexPattleColor3D",
                 "compositor/main_forward_unlit_pattle.vert.glsl",
                 "compositor/main_forward_unlit_vertexcolor.frag.glsl",
                 "surface/unlit_vertexcolor_surface.glsl"));

    // ------------------------------------------------------------------
    // 3D Unlit: Gizmo
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Unlit, GM::Mesh3D, TSM::None, VF_DebugShading),
        MakeDesc("Gizmo3D",
                 "compositor/main_forward_unlit_normal.vert.glsl",
                 "compositor/main_forward_unlit_normal.frag.glsl",
                 "surface/gizmo3d_surface.glsl"));

    // ------------------------------------------------------------------
    // Billboard Dynamic (camera-facing, transparent)
    // ------------------------------------------------------------------
    {
        MaterialVariantKey key;
        key.geometry_mode       = GM::BillboardCameraFacing;
        key.texture_source_mode = TSM::Tex2D;
        key.feature_bits        = VF_HasBaseColorTex;
        key.blend_mode          = BlendMode::Transparent;
        key.pass_hint           = PassType::ForwardTransparent;
        RegisterVariant(key,
            MakeDesc("Billboard2DDynamic",
                     "compositor/main_forward_billboard_dynamic.vert.glsl",
                     "compositor/main_forward_billboard.frag.glsl",
                     "surface/billboard_texture_surface.glsl"));
    }

    // ------------------------------------------------------------------
    // Billboard Fixed Size (axis-locked, transparent)
    // ------------------------------------------------------------------
    {
        MaterialVariantKey key;
        key.geometry_mode       = GM::BillboardAxisLocked;
        key.texture_source_mode = TSM::Tex2D;
        key.feature_bits        = VF_HasBaseColorTex;
        key.blend_mode          = BlendMode::Transparent;
        key.pass_hint           = PassType::ForwardTransparent;
        RegisterVariant(key,
            MakeDesc("Billboard2DFixed",
                     "compositor/main_forward_billboard_fixed.vert.glsl",
                     "compositor/main_forward_billboard.frag.glsl",
                     "surface/billboard_texture_surface.glsl"));
    }

    // ------------------------------------------------------------------
    // Terrain
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Terrain, GM::Mesh3D),
        MakeDesc("TerrainGrid",
                 "compositor/main_terrain_grid.vert.glsl",
                 "compositor/main_terrain_grid.frag.glsl",
                 "surface/terrain_grid_surface.glsl"));

    // ------------------------------------------------------------------
    // Sky
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Sky, GM::Mesh3D),
        MakeDesc("SkyMinimal",
                 "compositor/main_forward_sky.vert.glsl",
                 "compositor/main_forward_sky.frag.glsl",
                 "surface/sky_minimal_surface.glsl"));

    // ------------------------------------------------------------------
    // Standard 3D (texture-based, lit)
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Standard, GM::Mesh3D, TSM::Tex2D,
          VF_HasBaseColorTex | VF_HasNormalTex | VF_HasRoughnessTex),
        MakeDesc("Standard",
                 "compositor/main_forward_lit.vert.glsl",
                 "compositor/main_forward_lit.frag.glsl",
                 "surface/standard_surface.glsl"));

    // ------------------------------------------------------------------
    // Standard Texture Array
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Standard, GM::Mesh3D, TSM::Tex2DArray,
          VF_HasBaseColorTex | VF_HasNormalTex),
        MakeDesc("StandardTextureArray",
                 "compositor/main_forward_lit.vert.glsl",
                 "compositor/main_forward_lit.frag.glsl",
                 "surface/standard_texturearray_surface.glsl"));

    // ------------------------------------------------------------------
    // PBRColor3D (Standard surface, color-only via MaterialInstance)
    // ------------------------------------------------------------------
    RegisterVariant(
        K(ST::Standard, GM::Mesh3D),
        MakeDesc("PBRColor3D",
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
