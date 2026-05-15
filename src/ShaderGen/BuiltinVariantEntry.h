// BuiltinVariantEntry.h  —  shared internal header for the built-in variant table
//
// Defines the BuiltinVariantEntry POD struct together with its BuildKey() / BuildDesc()
// helpers and the canonical kBuiltinVariants[] declaration.
//
// USAGE:
//   VariantRegistry.cpp  — defines kBuiltinVariants[] and calls BuildKey()/BuildDesc()
//   MaterialLibrary.cpp  — scans kBuiltinVariants[] in RouteKey() to select the base entry
//                          for a preset before applying runtime overrides.
//
// This file intentionally has no include guards of its own (use #pragma once at each
// including TU) and relies on the including TU to have pulled in the MTL/SurfaceType etc.
// headers already.
// ---------------------------------------------------------------------------
#pragma once

#include<hgl/mtl/MaterialVariantKey.h>
#include<hgl/mtl/MaterialPreset.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<hgl/mtl/MaterialVariantRow.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/mtl/SurfaceType.h>
#include<hgl/mtl/LightingModel.h>
#include<hgl/mtl/RenderAlphaMode.h>
#include<hgl/mtl/PassType.h>
#include<hgl/mtl/SkyLight.h>

namespace hgl::graph::mtl {

// ---------------------------------------------------------------------------
// Compact type aliases (used inside kBuiltinVariants[] initialisers)
// ---------------------------------------------------------------------------
using _BVE_ST   = SurfaceType;
using _BVE_GM   = GeometryMode;
using _BVE_TSM  = TextureSourceMode;
using _BVE_LM   = LightingModel;
using _BVE_RM   = RenderAlphaMode;
using _BVE_PT   = PassType;
using _BVE_Slot = SamplerSlot;

inline constexpr uint32 _BVE_VA(VertexAttrib a) { return VertexAttribFeatureBit(a); }

// ---------------------------------------------------------------------------
// BuiltinVariantEntry  —  flat POD row for one registered shader variant
// ---------------------------------------------------------------------------
struct TexMode
{
    SamplerSlot       slot = _BVE_Slot::BaseColor;
    TextureSourceMode mode = _BVE_TSM::None;
};

struct BuiltinVariantEntry
{
    const char*          name;
    MaterialPreset       preset;

    SurfaceType          surface_type     = _BVE_ST::Unlit;
    GeometryMode         geometry_mode    = _BVE_GM::Mesh3D;
    PositionProviderId   position_provider = PositionProviderId::DirectVec3;
    LightingModel        lighting         = _BVE_LM::Lambert;
    SkyLightAmbientModel sky_model     = SkyLightAmbientModel::Simple;
    RenderAlphaMode      blend         = _BVE_RM::Opaque;
    PassType             pass          = _BVE_PT::ForwardOpaque;
    uint32               vertex_bits   = 0;
    uint32               extra_bits    = 0;
    TexMode              tex[4]        = {};   // zero-init → {BaseColor, None} → skipped

    const char*          vs_path       = "";
    const char*          fs_path       = "";
    const char*          surface_path  = "";

    // Phase 3: when false, sky_model is resource-policy only and does NOT affect
    // the registry lookup key or row-hash matching. Set true only for rows that
    // actually have distinct shader templates per sky model.
    bool                 sky_is_routing_axis = false;
};

// ---------------------------------------------------------------------------
// BuildKey / BuildDesc  —  produce the registry key / descriptor from an entry
// ---------------------------------------------------------------------------
inline MaterialVariantKey BuildKey(const BuiltinVariantEntry &e)
{
    MaterialVariantKey k;
    uint64 row_hash = hgl::hash::FNV1aInit<uint64>();
    if (e.name)
    {
        for (const char *p = e.name; *p; ++p)
            row_hash = hgl::hash::FNV1aAppend(row_hash, static_cast<uchar>(*p));
    }

    k.variant_row_name_hash         = row_hash;
    k.surface_type                  = e.surface_type;
    k.geometry_mode                 = e.geometry_mode;
    k.position_provider             = e.position_provider;
    k.vertex_attribute_feature_bits = e.vertex_bits;
    k.extra_feature_bits            = e.extra_bits;
    k.blend_mode                    = e.blend;
    k.pass_hint                     = e.pass;
    k.lighting_model                = e.lighting;
    // Phase 3: sky_ambient_model only participates in key identity when the row
    // explicitly opts in via sky_is_routing_axis. Otherwise canonicalize to Simple
    // so all sky model values match the same row.
    k.sky_ambient_model             = e.sky_is_routing_axis
                                      ? e.sky_model
                                      : SkyLightAmbientModel::Simple;
    for (const auto &tm : e.tex)
        if (tm.mode != _BVE_TSM::None)
            k.SetTextureSourceMode(tm.slot, tm.mode);
    return k;
}

inline MaterialVariantDesc BuildDesc(const BuiltinVariantEntry &e)
{
    MaterialVariantDesc d;
    d.variant_name          = e.name;
    d.factory_type          = e.preset;
    d.vs_template_path      = e.vs_path;
    d.fs_template_path      = e.fs_path;
    d.surface_function_path = e.surface_path;
    return d;
}

inline MaterialVariantRow BuildRowFromBuiltinVariantEntry(const BuiltinVariantEntry &e)
{
    MaterialVariantRow row;
    row.name = e.name;
    row.preset = e.preset;
    row.factory_type = e.preset;
    row.surface_type = e.surface_type;
    row.geometry_mode = e.geometry_mode;
    row.position_provider = e.position_provider;
    row.blend = e.blend;
    row.pass = e.pass;
    row.vs_template_path = e.vs_path;
    row.fs_template_path = e.fs_path;
    row.surface_path = e.surface_path;
    row.resources.lighting_model = e.lighting;
    row.resources.sky_model = e.sky_model;
    row.sky_is_routing_axis = e.sky_is_routing_axis;

    switch (e.preset)
    {
    case MaterialPreset::VertexColor2D:
        row.primitive = PrimitiveType::Triangles;
        row.vertex_input = VertexInputProfile::Position2D;
        row.vertex_policy = VertexTransformPolicy::Quad2D;
        row.surface_model = SurfaceShadingModel::VertexColor;
        row.vs_features.SetVertexAttrib(VertexAttrib::Position);
        row.vs_features.SetVertexAttrib(VertexAttrib::Color);
        row.fs_features.SetVertexAttrib(VertexAttrib::Color);
        row.def_hint = StaticMaterialDefIdHint::Standard2D;
        break;

    case MaterialPreset::PureColor2D:
        row.primitive = PrimitiveType::Triangles;
        row.vertex_input = VertexInputProfile::Position2D;
        row.vertex_policy = VertexTransformPolicy::Quad2D;
        row.surface_model = SurfaceShadingModel::PureColor;
        row.vs_features.SetVertexAttrib(VertexAttrib::Position);
        row.fs_features.SetVertexAttrib(VertexAttrib::Position);
        row.resources.needs_material_instance = true;
        row.schema = ShaderDataSchema::Color4f;
        row.def_hint = StaticMaterialDefIdHint::Standard2D;
        break;

    case MaterialPreset::PureTexture2D:
        row.primitive = PrimitiveType::Triangles;
        row.vertex_input = VertexInputProfile::PositionTexCoord2D;
        row.vertex_policy = VertexTransformPolicy::Quad2D;
        row.surface_model = SurfaceShadingModel::Texture2D;
        row.vs_features.SetVertexAttrib(VertexAttrib::Position);
        row.vs_features.SetVertexAttrib(VertexAttrib::TexCoord);
        row.fs_features.SetVertexAttrib(VertexAttrib::TexCoord);
        row.texture_count = 1;
        row.textures[0].slot = SamplerSlot::BaseColor;
        row.textures[0].source_mode = e.tex[0].mode;
        row.textures[0].sampler_type = e.tex[0].mode == TextureSourceMode::Array ? SamplerType::Sampler2DArray : SamplerType::Sampler2D;
        row.def_hint = StaticMaterialDefIdHint::Standard2D;
        break;

    case MaterialPreset::Text2D:
        row.primitive = PrimitiveType::Triangles;
        row.vertex_input = VertexInputProfile::PositionTexCoord2D;
        row.vertex_policy = VertexTransformPolicy::Text2D;
        row.surface_model = SurfaceShadingModel::Text;
        row.vs_features.SetVertexAttrib(VertexAttrib::Position);
        row.vs_features.SetVertexAttrib(VertexAttrib::TexCoord);
        row.fs_features.SetVertexAttrib(VertexAttrib::TexCoord);
        row.texture_count = 1;
        row.textures[0].slot = SamplerSlot::Text;
        row.textures[0].source_mode = TextureSourceMode::Atlas;
        row.textures[0].sampler_type = SamplerType::Sampler2D;
        row.resources.needs_material_instance = true;
        row.schema = ShaderDataSchema::TextColor;
        row.def_hint = StaticMaterialDefIdHint::Text2D;
        break;

    case MaterialPreset::PureColor3D:
        row.primitive = PrimitiveType::Triangles;
        row.vertex_input = VertexInputProfile::Position3D;
        row.vertex_policy = VertexTransformPolicy::Mesh3D;
        row.surface_model = SurfaceShadingModel::PureColor;
        row.vs_features.SetVertexAttrib(VertexAttrib::Position);
        row.fs_features.SetVertexAttrib(VertexAttrib::Position);
        row.resources.needs_viewport = true;
        row.resources.needs_camera = true;
        row.resources.needs_transform = true;
        row.resources.needs_material_instance = true;
        row.schema = ShaderDataSchema::Color4f;
        row.def_hint = StaticMaterialDefIdHint::PureColor3D;
        break;

    case MaterialPreset::VertexColor3D:
        row.primitive = PrimitiveType::Triangles;
        row.vertex_input = VertexInputProfile::PositionColor3D;
        row.vertex_policy = VertexTransformPolicy::Mesh3D;
        row.surface_model = SurfaceShadingModel::VertexColor;
        row.vs_features.SetVertexAttrib(VertexAttrib::Position);
        row.vs_features.SetVertexAttrib(VertexAttrib::Color);
        row.fs_features.SetVertexAttrib(VertexAttrib::Color);
        row.resources.needs_viewport = true;
        row.resources.needs_camera = true;
        row.resources.needs_transform = true;
        row.def_hint = StaticMaterialDefIdHint::VertexColor3D;
        break;

    case MaterialPreset::VertexLuminance3D:
        row.primitive = PrimitiveType::Triangles;
        row.vertex_input = VertexInputProfile::PositionLuminance3D;
        row.vertex_policy = VertexTransformPolicy::Mesh3D;
        row.surface_model = SurfaceShadingModel::VertexLuminance;
        row.vs_features.SetVertexAttrib(VertexAttrib::Position);
        row.vs_features.SetVertexAttrib(VertexAttrib::Luminance);
        row.fs_features.SetVertexAttrib(VertexAttrib::Luminance);
        row.resources.needs_viewport = true;
        row.resources.needs_camera = true;
        row.resources.needs_transform = true;
        row.resources.needs_material_instance = true;
        row.schema = ShaderDataSchema::Color4f;
        row.def_hint = StaticMaterialDefIdHint::VertexLuminance3D;
        break;

    case MaterialPreset::VertexLuminance2D:
        row.primitive = PrimitiveType::Triangles;
        row.vertex_input = VertexInputProfile::PositionLuminance2D;
        row.vertex_policy = VertexTransformPolicy::Quad2D;
        row.surface_model = SurfaceShadingModel::VertexLuminance;
        row.vs_features.SetVertexAttrib(VertexAttrib::Position);
        row.vs_features.SetVertexAttrib(VertexAttrib::Luminance);
        row.fs_features.SetVertexAttrib(VertexAttrib::Luminance);
        row.resources.needs_material_instance = true;
        row.schema = ShaderDataSchema::Color4f;
        row.def_hint = StaticMaterialDefIdHint::Standard2D;
        break;

    case MaterialPreset::VertexPaletteColor3D:
        row.primitive = PrimitiveType::Triangles;
        row.vertex_input = VertexInputProfile::PositionPaletteIndex3D;
        row.vertex_policy = VertexTransformPolicy::Mesh3D;
        row.surface_model = SurfaceShadingModel::VertexColor;
        row.vs_features.SetVertexAttrib(VertexAttrib::Position);
        row.vs_features.SetVertexAttrib(VertexAttrib::Color);
        row.fs_features.SetVertexAttrib(VertexAttrib::Color);
        row.resources.needs_viewport = true;
        row.resources.needs_camera = true;
        row.resources.needs_transform = true;
        row.resources.needs_color_palette = true;
        row.def_hint = StaticMaterialDefIdHint::VertexPaletteColor3D;
        break;

    case MaterialPreset::Gizmo3D:
        row.primitive = PrimitiveType::Triangles;
        row.vertex_input = VertexInputProfile::PositionNormal3D;
        row.vertex_policy = VertexTransformPolicy::Mesh3D;
        row.surface_model = SurfaceShadingModel::Gizmo;
        row.vs_features.SetVertexAttrib(VertexAttrib::Position);
        row.vs_features.SetVertexAttrib(VertexAttrib::Normal);
        row.fs_features.SetVertexAttrib(VertexAttrib::Position);
        row.fs_features.SetVertexAttrib(VertexAttrib::Normal);
        row.resources.needs_viewport = true;
        row.resources.needs_camera = true;
        row.resources.needs_transform = true;
        row.resources.needs_material_instance = true;
        row.schema = ShaderDataSchema::Color4f;
        row.def_hint = StaticMaterialDefIdHint::Gizmo3D;
        break;

    case MaterialPreset::UnlitTexture3D:
        row.primitive = PrimitiveType::Triangles;
        row.vertex_input = VertexInputProfile::PositionTexCoord3D;
        row.vertex_policy = VertexTransformPolicy::Mesh3D;
        row.surface_model = SurfaceShadingModel::UnlitTexture3D;
        row.vs_features.SetVertexAttrib(VertexAttrib::Position);
        row.vs_features.SetVertexAttrib(VertexAttrib::TexCoord);
        row.fs_features.SetVertexAttrib(VertexAttrib::TexCoord);
        row.resources.needs_viewport = true;
        row.resources.needs_camera = true;
        row.resources.needs_transform = true;
        row.texture_count = 1;
        row.textures[0].slot = SamplerSlot::BaseColor;
        row.textures[0].source_mode = e.tex[0].mode;
        row.textures[0].sampler_type = e.tex[0].mode == TextureSourceMode::Array
                                       ? SamplerType::Sampler2DArray
                                       : SamplerType::Sampler2D;
        row.def_hint = StaticMaterialDefIdHint::UnlitTexture3D;
        break;

    case MaterialPreset::Billboard2DDynamic:
        row.primitive = PrimitiveType::Billboard;
        row.vertex_input = VertexInputProfile::BillboardPositionOnly3D;
        row.vertex_policy = VertexTransformPolicy::BillboardCameraFacing;
        row.surface_model = SurfaceShadingModel::BillboardTexture;
        row.vs_features.SetVertexAttrib(VertexAttrib::Position);
        row.fs_features.SetVertexAttrib(VertexAttrib::TexCoord);
        row.resources.needs_viewport = true;
        row.resources.needs_camera = true;
        row.resources.needs_transform = true;
        row.resources.needs_material_instance = true;
        row.texture_count = 1;
        row.textures[0].slot = SamplerSlot::BaseColor;
        row.textures[0].source_mode = e.tex[0].mode;
        row.textures[0].sampler_type = SamplerType::Sampler2D;
        row.schema = ShaderDataSchema::BillboardSizeUVec2;
        row.def_hint = StaticMaterialDefIdHint::BillboardDynamic;
        break;

    case MaterialPreset::Billboard2DFixed:
        row.primitive = PrimitiveType::Billboard;
        row.vertex_input = VertexInputProfile::BillboardPositionOnly3D;
        row.vertex_policy = VertexTransformPolicy::BillboardAxisLocked;
        row.surface_model = SurfaceShadingModel::BillboardTexture;
        row.vs_features.SetVertexAttrib(VertexAttrib::Position);
        row.fs_features.SetVertexAttrib(VertexAttrib::TexCoord);
        row.resources.needs_viewport = true;
        row.resources.needs_camera = true;
        row.resources.needs_transform = true;
        row.resources.needs_material_instance = true;
        row.texture_count = 1;
        row.textures[0].slot = SamplerSlot::BaseColor;
        row.textures[0].source_mode = e.tex[0].mode;
        row.textures[0].sampler_type = SamplerType::Sampler2D;
        row.schema = ShaderDataSchema::BillboardSizeUVec2;
        row.def_hint = StaticMaterialDefIdHint::BillboardFixed;
        break;

    case MaterialPreset::TerrainGrid:
        row.primitive = PrimitiveType::Triangles;
        row.vertex_input = VertexInputProfile::Position3D;
        row.vertex_policy = VertexTransformPolicy::TerrainGrid;
        row.surface_model = SurfaceShadingModel::TerrainGrid;
        row.vs_features.SetVertexAttrib(VertexAttrib::Position);
        row.fs_features.SetVertexAttrib(VertexAttrib::Normal);
        row.fs_features.has_clip_pos = true;
        row.resources.needs_viewport = true;
        row.resources.needs_camera = true;
        row.resources.needs_transform = true;
        row.texture_count = 2;
        row.textures[0].slot = SamplerSlot::Height;
        row.textures[0].source_mode = TextureSourceMode::Simple;
        row.textures[0].sampler_type = SamplerType::Sampler2D;
        row.textures[1].slot = SamplerSlot::Normal;
        row.textures[1].source_mode = TextureSourceMode::Simple;
        row.textures[1].sampler_type = SamplerType::Sampler2D;
        row.def_hint = StaticMaterialDefIdHint::TerrainGrid;
        break;

    case MaterialPreset::SkyMinimal:
        row.primitive = PrimitiveType::Triangles;
        row.vertex_input = VertexInputProfile::Position3D;
        row.vertex_policy = VertexTransformPolicy::Sky;
        row.surface_model = SurfaceShadingModel::SkyMinimal;
        row.vs_features.SetVertexAttrib(VertexAttrib::Position);
        row.vs_features.has_direction = true;
        row.fs_features.has_direction = true;
        row.resources.needs_viewport = true;
        row.resources.needs_camera = true;
        row.resources.needs_sky = true;
        row.resources.needs_transform = true;
        row.def_hint = StaticMaterialDefIdHint::SkyMinimal;
        break;

    case MaterialPreset::Standard:
    case MaterialPreset::HumanSkin:
    case MaterialPreset::AmphibiansSkin:
    case MaterialPreset::Wood:
    case MaterialPreset::TreeBark:
    case MaterialPreset::Stone:
    case MaterialPreset::Leaf:
    case MaterialPreset::Metal:
    case MaterialPreset::BirdFeathers:
    case MaterialPreset::Scales:
        row.primitive = PrimitiveType::Triangles;
        row.vertex_input = VertexInputProfile::PositionNormal3D;
        row.vertex_policy = VertexTransformPolicy::Mesh3D;
        row.surface_model = e.lighting == LightingModel::BlinnPhong
            ? SurfaceShadingModel::StandardBlinnPhong
            : (e.lighting == LightingModel::PBR ? SurfaceShadingModel::StandardPBR : SurfaceShadingModel::StandardLambert);
        row.vs_features.SetVertexAttrib(VertexAttrib::Position);
        row.vs_features.SetVertexAttrib(VertexAttrib::Normal);
        row.fs_features.SetVertexAttrib(VertexAttrib::Position);
        row.fs_features.SetVertexAttrib(VertexAttrib::Normal);
        row.resources.needs_viewport = true;
        row.resources.needs_camera = true;
        row.resources.needs_sky = true;
        row.resources.needs_transform = true;
        row.resources.needs_material_instance = true;
        row.resources.enable_lighting = true;
        row.texture_count = 2;
        row.textures[0].slot = e.tex[0].slot;
        row.textures[0].source_mode = e.tex[0].mode;
        row.textures[0].sampler_type = e.tex[0].mode == TextureSourceMode::Array ? SamplerType::Sampler2DArray : SamplerType::Sampler2D;
        row.textures[1].slot = e.tex[1].slot;
        row.textures[1].source_mode = e.tex[1].mode;
        row.textures[1].sampler_type = e.tex[1].mode == TextureSourceMode::Array ? SamplerType::Sampler2DArray : SamplerType::Sampler2D;
        row.schema = ShaderDataSchema::StandardParams;
        row.def_hint = StaticMaterialDefIdHint::Standard3D;
        break;

    case MaterialPreset::PBRColor3D:
        row.primitive = PrimitiveType::Triangles;
        row.vertex_input = VertexInputProfile::PositionNormal3D;
        row.vertex_policy = VertexTransformPolicy::Mesh3D;
        row.surface_model = SurfaceShadingModel::PBRColor;
        row.vs_features.SetVertexAttrib(VertexAttrib::Position);
        row.vs_features.SetVertexAttrib(VertexAttrib::Normal);
        row.fs_features.SetVertexAttrib(VertexAttrib::Position);
        row.fs_features.SetVertexAttrib(VertexAttrib::Normal);
        row.resources.needs_viewport = true;
        row.resources.needs_camera = true;
        row.resources.needs_sky = true;
        row.resources.needs_transform = true;
        row.resources.needs_material_instance = true;
        row.resources.enable_lighting = true;
        row.schema = ShaderDataSchema::PBRColorParams;
        row.def_hint = StaticMaterialDefIdHint::Standard3D;
        break;

    case MaterialPreset::FullscreenTriangle:
        row.primitive = PrimitiveType::Triangles;
        row.vertex_input = VertexInputProfile::FullscreenProcedural;
        row.vertex_policy = VertexTransformPolicy::FullscreenTriangle;
        row.surface_model = SurfaceShadingModel::Unknown;
        row.def_hint = StaticMaterialDefIdHint::FullscreenTriangle;
        break;

    case MaterialPreset::Checkerboard3D:
        row.primitive = PrimitiveType::Triangles;
        row.vertex_input = VertexInputProfile::Position3D;
        row.vertex_policy = VertexTransformPolicy::Mesh3D;
        row.surface_model = SurfaceShadingModel::CheckerboardFallback;
        row.def_hint = StaticMaterialDefIdHint::None;
        break;

    default:
        break;
    }

    return row;
}

// ---------------------------------------------------------------------------
// kBuiltinVariants  —  defined in VariantRegistry.cpp; visible here for callers
//                       that need to scan the table (e.g. MaterialLibrary.cpp)
// ---------------------------------------------------------------------------
extern const BuiltinVariantEntry kBuiltinVariants[];
extern const size_t              kBuiltinVariantsCount;
} // namespace hgl::graph::mtl
