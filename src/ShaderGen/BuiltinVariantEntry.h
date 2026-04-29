// BuiltinVariantEntry.h  —  shared internal header for the built-in variant table
//
// Defines the BuiltinVariantEntry POD struct together with its BuildKey() / BuildDesc()
// helpers and the canonical kBuiltinVariants[] declaration.
//
// USAGE:
//   VariantRegistry.cpp  — defines kBuiltinVariants[] and calls BuildKey()/BuildDesc()
//   MaterialLibrary.cpp  — scans kBuiltinVariants[] in RouteKey() to eliminate the old
//                          per-preset MakeXxxKey() function-pointer dispatch.
//
// This file intentionally has no include guards of its own (use #pragma once at each
// including TU) and relies on the including TU to have pulled in the MTL/SurfaceType etc.
// headers already.
// ---------------------------------------------------------------------------
#pragma once

#include<hgl/mtl/MaterialVariantKey.h>
#include<hgl/mtl/MaterialPreset.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/mtl/SurfaceType.h>
#include<hgl/common/PositionType.h>
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
inline constexpr uint32 _BVE_EX(ExtraFeature  f) { return static_cast<uint32>(f); }

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

    SurfaceType          surface_type  = _BVE_ST::Unlit;
    GeometryMode         geometry_mode = _BVE_GM::Mesh3D;
    PositionType         position_type = PositionType::Vec3;
    LightingModel        lighting      = _BVE_LM::Lambert;
    SkyLightAmbientModel sky_model     = SkyLightAmbientModel::Simple;
    RenderAlphaMode      blend         = _BVE_RM::Opaque;
    PassType             pass          = _BVE_PT::ForwardOpaque;
    uint32               vertex_bits   = 0;
    uint32               extra_bits    = 0;
    TexMode              tex[4]        = {};   // zero-init → {BaseColor, None} → skipped

    const char*          vs_path       = "";
    const char*          fs_path       = "";
    const char*          surface_path  = "";
};

// ---------------------------------------------------------------------------
// BuildKey / BuildDesc  —  produce the registry key / descriptor from an entry
// ---------------------------------------------------------------------------
inline MaterialVariantKey BuildKey(const BuiltinVariantEntry &e)
{
    MaterialVariantKey k;
    k.surface_type                  = e.surface_type;
    k.geometry_mode                 = e.geometry_mode;
    k.position_type                 = e.position_type;
    k.vertex_attribute_feature_bits = e.vertex_bits;
    k.extra_feature_bits            = e.extra_bits;
    k.blend_mode                    = e.blend;
    k.pass_hint                     = e.pass;
    k.lighting_model                = e.lighting;
    k.sky_ambient_model             = e.sky_model;
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

// ---------------------------------------------------------------------------
// kBuiltinVariants  —  defined in VariantRegistry.cpp; visible here for callers
//                       that need to scan the table (e.g. MaterialLibrary.cpp)
// ---------------------------------------------------------------------------
extern const BuiltinVariantEntry kBuiltinVariants[];
extern const size_t              kBuiltinVariantsCount;

} // namespace hgl::graph::mtl
