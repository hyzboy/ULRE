#include "VariantRoutingPolicy.h"

#include "../BuiltinVariantEntry.h"

#include <cstdio>

namespace hgl::graph::mtl::routing
{

namespace
{

struct PresetResolveEntry
{
    MaterialPreset preset;
    const char *name;
};

static const PresetResolveEntry kPresetResolveTable[] =
{
    // Canonical presets (one entry per kBuiltinVariants preset)
    {MaterialPreset::Checkerboard3D,       "Checkerboard3D"}, // alias -> Standard via IsSemanticMaterialPreset
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

static const PresetResolveEntry *FindPresetResolveEntry(MaterialPreset preset) noexcept
{
    for (const auto &entry : kPresetResolveTable)
        if (entry.preset == preset)
            return &entry;

    return nullptr;
}

} // namespace

bool IsSemanticMaterialPreset(MaterialPreset preset) noexcept
{
    switch (preset)
    {
        case MaterialPreset::Checkerboard3D: // alias -> Standard
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

MaterialPreset ResolvePresetForLOD(MaterialPreset preset,
                                   MaterialLOD lod) noexcept
{
    switch (lod)
    {
        case MaterialLOD::Base:
        default:
            // Current bootstrap behavior: semantic presets still reuse the Standard family.
            if (IsSemanticMaterialPreset(preset))
                return MaterialPreset::Standard;

            return preset;
    }
}

const char *GetPresetName(MaterialPreset preset) noexcept
{
    const PresetResolveEntry *entry = FindPresetResolveEntry(preset);
    return entry ? entry->name : nullptr;
}

MaterialVariantKey BuildRouteKey(MaterialPreset preset,
                                 uint32 extra_attrib_bits,
                                 const RuntimeKeyOverrides &ov) noexcept
{
    // Step 1: resolve semantic alias -> canonical preset via LOD table.
    const MaterialPreset resolved_preset =
        ResolvePresetForLOD(preset, GetDefaultMaterialLOD());

    // Step 2: scan kBuiltinVariants for the best matching entry.
    //   - If ov.blend_mode is set, select the entry whose blend field matches.
    //   - If ov.lighting_model is set, additionally filter on lighting field.
    //   - First match wins (table entries ordered from most common to rarest).
    const BuiltinVariantEntry *found = nullptr;
    for (size_t i = 0; i < kBuiltinVariantsCount; ++i)
    {
        const auto &e = kBuiltinVariants[i];
        if (e.preset != resolved_preset)                            continue;
        if (ov.blend_mode     && e.blend    != *ov.blend_mode)      continue;
        if (ov.lighting_model && e.lighting != *ov.lighting_model)  continue;
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
    if (ov.position_provider) key.position_provider = *ov.position_provider;
    if (ov.pass_hint)         key.pass_hint         = *ov.pass_hint;
    if (ov.sky_ambient_model) key.sky_ambient_model = *ov.sky_ambient_model;
    if (ov.debug_shading)     key.SetDebugShading(true);

    // Phase C: apply per-semantic attribute provider overrides (vertex pulling).
    for (size_t i = 0; i < ov.attribute_providers.size(); ++i)
        if (ov.attribute_providers[i] != AttributeProviderId::None)
            key.attribute_providers[i] = ov.attribute_providers[i];

    return key;
}

} // namespace hgl::graph::mtl::routing
