/// RecipeToKey.cpp — implementation of ResolveRecipePrimaryKey()
///
/// Centralizes all Recipe → MaterialVariantKey canonicalization logic that was
/// previously scattered across five call sites:
///   1. MaterialAssetLoader::CreateMaterialFromRecord (header-only)
///   2. MaterialFactory3D::Create → M_*.cpp::Create*Variant
///   3. MaterialLibrary::RouteKey
///   4. MaterialLibrary::ApplyCreateConfigToVariantKey
///   5. *VariantRouter::Build*Policy
///
/// This file has NO glslang / SPIRV-Cross / file-I/O dependency.  Any change
/// that would add such a dependency must be rejected.

#include <hgl/mtl/RecipeToKey.h>
#include <hgl/mtl/MaterialLibrary.h>   // RouteKey
#include <hgl/mtl/MaterialFeature.h>   // ResolveIntentFeatureMask, ResolveLightingModelFromFeatures
#include <hgl/mtl/PassExpansion.h>
#include <hgl/mtl/MaterialKeyToolchainVersion.h>
#include <hgl/mtl/StaticMaterialDefRegistry.h>

#include "3d/StandardStaticDef.h"

namespace hgl::graph::mtl
{

// ─────────────────────────────────────────────────────────────────────────────
// File-local helpers (not exposed in the header)
// ─────────────────────────────────────────────────────────────────────────────

/// Returns true if any texture channel in the recipe uses Array source mode.
static bool HasAnyArrayTexture(const MaterialRecipe &r) noexcept
{
    for (const auto &tc : r.textures)
        if (tc.source_mode == TextureSourceMode::Array)
            return true;
    return false;
}

/// Lazy-init helper: acquire (and cache) the def ID for the Standard material.
/// @param any_array  true → Sampler2DArray variant; false → Sampler2D variant.
static StaticMaterialDefId GetStandardDefId(bool any_array) noexcept
{
    static StaticMaterialDefId s_id_2d    = kInvalidStaticMaterialDefId;
    static StaticMaterialDefId s_id_array = kInvalidStaticMaterialDefId;

    StaticMaterialDefId &slot = any_array ? s_id_array : s_id_2d;

    if (slot == kInvalidStaticMaterialDefId)
    {
        slot = AcquireStaticMaterialDefId(BuildCanonicalStandardStaticDef(any_array));
    }

    return slot;
}

/// Resolve the StaticMaterialDefId for the given recipe.
/// Returns kInvalidStaticMaterialDefId for presets that have no registered def.
static StaticMaterialDefId ResolveDefIdForRecipe(const MaterialRecipe &r) noexcept
{
    switch (r.preset)
    {
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
        return GetStandardDefId(HasAnyArrayTexture(r));

    default:
        return kInvalidStaticMaterialDefId;
    }
}

/// Map a preset to its default ShaderDataSchema.
///
/// TODO(Step-6): replace with StaticMaterialDef lookup once def_id is wired up.
/// Until then this table provides stable defaults for the key's schema field.
static ShaderDataSchema GetDefaultSchemaForPreset(const MaterialPreset p) noexcept
{
    switch (p)
    {
    case MaterialPreset::PureColor2D:
    case MaterialPreset::PureColor3D:
        return ShaderDataSchema::Color4f;

    case MaterialPreset::Text2D:
        return ShaderDataSchema::TextColor;

    case MaterialPreset::Billboard2DFixed:
    case MaterialPreset::Billboard2DDynamic:
        return ShaderDataSchema::BillboardSizeUVec2;

    case MaterialPreset::PBRColor3D:
        return ShaderDataSchema::PBRColorParams;

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
        return ShaderDataSchema::StandardParams;

    default:
        return ShaderDataSchema::None;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// detail namespace
// ─────────────────────────────────────────────────────────────────────────────

namespace detail
{

PassType GetPrimaryPassForBlendMode(RenderAlphaMode blend) noexcept
{
    const auto passes = GetPassTypesForBlendMode(blend);
    return passes.empty() ? PassType::ForwardOpaque : passes.front();
}

MaterialVariantKey BuildBaseVariantKeyFromRecipe(const MaterialRecipe &r) noexcept
{
    // ── Step 1: preset → base variant key ────────────────────────────────────
    // [Step 3.5 T1] Routes through the single RouteKey() entry.
    // Behaviour is identical
    // (LOD resolution + canonical preset aliasing) because RouteKey() applies
    // the same PresetResolveTable.
    MaterialVariantKey k = RouteKey(r.preset);

    // ── Step 2: dimension ─────────────────────────────────────────────────────
    // D2 materials use a quad geometry, not a generic 3-D mesh.
    if (r.dim == MaterialRecipe::Dim::D2)
        k.geometry_mode = GeometryMode::Quad2D;

    // ── Step 3: vertex position format ───────────────────────────────────────
    // Mirrors Material2DCreateConfig::position_format in ApplyCreateConfigToVariantKey.
    if (r.pos_format.Check())
    {
        k.position_provider = (r.pos_format.vec_size == 2)
            ? PositionProviderId::VAB_Vec2
            : PositionProviderId::DirectVec3;
    }

    // ── Step 3.B: explicit position provider override (Phase C) ──────────────
    if (r.position_provider.has_value())
        k.position_provider = *r.position_provider;

    // ── Step 3.C: attribute provider overrides (vertex pulling, Phase C) ──────
    for (size_t i = 0; i < size_t(AttributeSemantic::BuiltinCount); ++i)
        if (r.attribute_providers[i] != AttributeProviderId::None)
            k.attribute_providers[i] = r.attribute_providers[i];

    // ── Step 4: sky / lighting (3-D only) ────────────────────────────────────
    // Mirrors the Material3DCreateConfig block in ApplyCreateConfigToVariantKey
    // and the CreateMaterialFromRecord 3D path in MaterialAssetLoader.
    if (r.dim == MaterialRecipe::Dim::D3)
    {
        k.sky_ambient_model = r.sky_ambient;

        const MaterialFeatureMask fmask =
            ResolveIntentFeatureMask(r.preset, r.intent_features);
        k.effective_feature_mask = fmask;
        k.lighting_model =
            ResolveLightingModelFromFeatures(fmask, LightingModel::Lambert);
    }

    // ── Step 5: texture source-mode overrides ─────────────────────────────────
    // Mirrors the SetTextureSourceModeOverride loop in CreateMaterialFromRecord.
    for (const auto &tc : r.textures)
    {
        if (tc.source_mode != TextureSourceMode::None)
            k.SetTextureSourceMode(tc.slot, tc.source_mode);
    }

    // ── Step 6: billboard geometry mode ──────────────────────────────────────
    // Ensure the correct geometry mode regardless of the dim override above.
    // Mirrors M_BillboardFixedSize / M_BillboardDynamicSize lookup key setup.
    if (r.preset == MaterialPreset::Billboard2DFixed)
        k.geometry_mode = GeometryMode::BillboardAxisLocked;
    else if (r.preset == MaterialPreset::Billboard2DDynamic)
        k.geometry_mode = GeometryMode::BillboardCameraFacing;

    // ── Step 7: billboard blend mode ─────────────────────────────────────────
    // Billboard presets allow the caller to override the default Transparent
    // blend_mode via BillboardConfig::blend_mode.
    // Non-billboard presets inherit their blend_mode from RouteKey preset defaults
    // (typically Opaque for 3-D presets, Transparent for billboard bases).
    if (r.preset == MaterialPreset::Billboard2DFixed
        || r.preset == MaterialPreset::Billboard2DDynamic)
    {
        k.blend_mode = r.billboard.blend_mode;
        k.pass_hint  = GetPrimaryPassForBlendMode(r.billboard.blend_mode);
    }

    return k;
}

MaterialVariantKey ApplyRouterCanonicalization(const MaterialVariantKey &in) noexcept
{
    MaterialVariantKey k = in;

    // Standard + Mesh3D: canonicalize sky_ambient_model to Simple.
    //
    // This rule originates from three independent call sites in the old path:
    //   - M_Standard.cpp:145    (StandardVariantRouter::BuildPolicy)
    //   - M_PBRColor3D.cpp:83   (PBRColor3D variant construction)
    //   - MaterialLibrary.cpp:477 (CreateMaterialCreateInfo registry lookup)
    //
    // Both Standard and PBRColor3D use surface_type = SurfaceType::Standard,
    // so one condition covers both presets.
    if (k.surface_type == SurfaceType::Standard
     && k.geometry_mode == GeometryMode::Mesh3D)
    {
        k.sky_ambient_model = SkyLightAmbientModel::Simple;
    }

    return k;
}

} // namespace detail

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

MaterialKey ResolveRecipePrimaryKey(const MaterialRecipe &r) noexcept
{
    MaterialKey k{};

    // Phase A: build un-canonicalized variant key
    MaterialVariantKey vk = detail::BuildBaseVariantKeyFromRecipe(r);

    // Phase B: apply router canonicalization
    vk = detail::ApplyRouterCanonicalization(vk);
    k.variant = vk;

    // Phase C: primary pass
    k.pass = detail::GetPrimaryPassForBlendMode(vk.blend_mode);

    // Phase D: schema
    k.schema = GetDefaultSchemaForPreset(r.preset);

    // Phase E: def_id
    k.def_id = ResolveDefIdForRecipe(r);

    // Phase F: toolchain versions
    k.glsl_version = kMaterialKeyGLSLVersion;
    k.vk_version   = kMaterialKeyVulkanVersion;
    k.spv_version  = kMaterialKeySpvVersion;

    return k;
}

std::vector<MaterialKey> EnumerateRecipeKeys(const MaterialRecipe &r)
{
    MaterialKey base = ResolveRecipePrimaryKey(r);
    auto passes = GetPassTypesForBlendMode(base.variant.blend_mode);

    std::vector<MaterialKey> out;
    out.reserve(passes.size());
    for (PassType pass : passes)
    {
        MaterialKey k = base;
        k.pass = pass;
        out.push_back(k);
    }
    return out;
}

} // namespace hgl::graph::mtl
