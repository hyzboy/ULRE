/// RecipeToKey.cpp — implementation of ResolveRecipePrimaryKey()
///
/// Centralizes all Recipe → MaterialVariantKey canonicalization logic that was
/// previously scattered across five call sites:
///   1. MaterialAssetLoader::CreateMaterialFromRecord (header-only)
///   2. MaterialFactory3D::Create → M_*.cpp::Create*Variant
///   3. MaterialLibrary::MapPresetToVariantKey
///   4. MaterialLibrary::ApplyCreateConfigToVariantKey
///   5. *VariantRouter::Build*Policy
///
/// This file has NO glslang / SPIRV-Cross / file-I/O dependency.  Any change
/// that would add such a dependency must be rejected.

#include <hgl/mtl/RecipeToKey.h>
#include <hgl/mtl/MaterialLibrary.h>   // MapPresetToVariantKey
#include <hgl/mtl/MaterialFeature.h>   // ResolveIntentFeatureMask, ResolveLightingModelFromFeatures
#include <hgl/mtl/PassExpansion.h>
#include <hgl/mtl/MaterialKeyToolchainVersion.h>
#include <hgl/mtl/StaticMaterialDefRegistry.h>
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/mtl/UBOCommon.h>

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
    // Static vertices and non-sampler descriptors match M_Standard.cpp exactly.
    static const FixedVertexEntry kStdVertex[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_VEC2, VAN::TexCoord },
        { VAT_VEC3, VAN::Normal   },
    };
    static const UBOSemanticSet kStdBaseUBOs = {
        UBODescriptorSemantic::ViewportInfo,
        UBODescriptorSemantic::CameraInfo,
        UBODescriptorSemantic::SkyInfo,
    };
    // Base SSBOs (without MaterialBindingInstanceTexture — added when any_array)
    static const SSBOSemanticSet kStdBaseSSBOs = {
        SSBODescriptorSemantic::TransformData,
        SSBODescriptorSemantic::TransformID,
        SSBODescriptorSemantic::MaterialBindingInstanceID,
        SSBODescriptorSemantic::MaterialBindingInstanceData,
    };
    static const SSBOSemanticSet kStdArraySSBOs = {
        SSBODescriptorSemantic::TransformData,
        SSBODescriptorSemantic::TransformID,
        SSBODescriptorSemantic::MaterialBindingInstanceID,
        SSBODescriptorSemantic::MaterialBindingInstanceData,
        SSBODescriptorSemantic::MaterialBindingInstanceTexture,
    };
    // Sampler2D variant
    static const StaticTextureSamplerDescriptors kStdSamplers2D = {
        { SamplerSlot::BaseColor, { SamplerType::Sampler2D, 0, 0, TextureChannelHint::RGBA } },
        { SamplerSlot::Normal,    { SamplerType::Sampler2D, 0, 0, TextureChannelHint::RGBA } },
    };
    // Sampler2DArray variant
    static const StaticTextureSamplerDescriptors kStdSamplers2DArray = {
        { SamplerSlot::BaseColor, { SamplerType::Sampler2DArray, 0, 0, TextureChannelHint::RGBA } },
        { SamplerSlot::Normal,    { SamplerType::Sampler2DArray, 0, 0, TextureChannelHint::RGBA } },
    };

    static StaticMaterialDefId s_id_2d    = kInvalidStaticMaterialDefId;
    static StaticMaterialDefId s_id_array = kInvalidStaticMaterialDefId;

    if (!any_array)
    {
        if (s_id_2d == kInvalidStaticMaterialDefId)
        {
            const StaticMaterialDef def {
                "Standard_v1",
                PrimitiveType::Triangles,
                kStdVertex,
                uint32_t(sizeof(kStdVertex) / sizeof(kStdVertex[0])),
                &kStdBaseUBOs,
                &kStdBaseSSBOs,
                &kStdSamplers2D,
                ShaderDataSchema::StandardParams,
            };
            s_id_2d = AcquireStaticMaterialDefId(def);
        }
        return s_id_2d;
    }
    else
    {
        if (s_id_array == kInvalidStaticMaterialDefId)
        {
            const StaticMaterialDef def {
                "StandardTextureArray_v1",
                PrimitiveType::Triangles,
                kStdVertex,
                uint32_t(sizeof(kStdVertex) / sizeof(kStdVertex[0])),
                &kStdBaseUBOs,
                &kStdArraySSBOs,
                &kStdSamplers2DArray,
                ShaderDataSchema::StandardParams,
            };
            s_id_array = AcquireStaticMaterialDefId(def);
        }
        return s_id_array;
    }
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
    // Mirrors the first entry returned by
    // CompositorAssembler::GetPassTypesForBlendMode.
    switch (blend)
    {
    case RenderAlphaMode::Opaque:          return PassType::ForwardOpaque;
    case RenderAlphaMode::Masked:          return PassType::ForwardMasked;
    case RenderAlphaMode::Transparent:     return PassType::ForwardTransparent;
    case RenderAlphaMode::Dither:          return PassType::ForwardDither;
    case RenderAlphaMode::AlphaToCoverage: return PassType::ForwardA2C;
    default:                               return PassType::ForwardOpaque;
    }
}

MaterialVariantKey BuildBaseVariantKeyFromRecipe(const MaterialRecipe &r) noexcept
{
    // ── Step 1: preset → base variant key ────────────────────────────────────
    // [Step 3.5 T1] Routes through the single RouteKey() entry instead of the
    // deprecated free function MapPresetToVariantKey. Behaviour is identical
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
            ? PositionProviderId::SSBO_PackedVec2
            : PositionProviderId::SSBO_PackedVec3;
    }

    // Default migration policy: route legacy VAB/direct position providers to
    // SSBO providers unless the recipe explicitly selects a provider.
    if (!r.position_provider.has_value())
    {
        if (k.position_provider == PositionProviderId::VAB_Vec2)
            k.position_provider = PositionProviderId::SSBO_PackedVec2;
        else if (k.position_provider == PositionProviderId::DirectVec3)
            k.position_provider = PositionProviderId::SSBO_PackedVec3;
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
    // Non-billboard presets inherit their blend_mode from MapPresetToVariantKey
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
