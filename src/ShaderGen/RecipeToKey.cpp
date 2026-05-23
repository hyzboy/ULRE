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
#include <hgl/mtl/MaterialVariantRow.h> // VertexTransformPolicy, SurfaceShadingModel
#include <hgl/mtl/GlobalRenderConfig.h>  // quality_level / render_phase for cache key
#include <hgl/mtl/RenderPhase.h>         // RenderPhase enum
#include <hgl/mtl/PassExpansion.h>
#include <hgl/mtl/MaterialKeyToolchainVersion.h>
#include <hgl/mtl/StaticMaterialDefRegistry.h>
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/mtl/MaterialVariantRegistry.h>
#include <hgl/shadergen/ColorSource.h>
#include <hgl/shadergen/ProviderManifest.h>
#include <cassert>

namespace hgl::graph::mtl
{
// ─────────────────────────────────────────────────────────────────────────────
// File-local helpers (not exposed in the header)
// ─────────────────────────────────────────────────────────────────────────────

static bool HasAnyArrayTexture(const MaterialRecipe &r) noexcept;

// ─────────────────────────────────────────────────────────────────────────────
// ResolveProviderFromDemand
//
// Table-driven: given recipe dim / vertex_policy, returns the VAB PositionProviderId
// that best represents the geometry.  This replaces the old hardcoded dim-switch
// in BuildBaseVariantKeyFromRecipe Step 1.
//
// Rules (in priority order):
//   1. Quad2D policy              → VAB_Vec2
//   2. dim == D2                  → VAB_Vec2
//   3. (default / D3)             → VAB_Vec3
//
// Future entries (VAB_Vec4, VAB_IVec2, …) can be added here without touching
// any other C++ routing code.
// ─────────────────────────────────────────────────────────────────────────────
static PositionProviderId ResolveProviderFromDemand(
    const MaterialPreset         /*preset*/,
    const MaterialRecipe::Dim    dim,
    const VertexTransformPolicy  vertex_policy) noexcept
{
    if (vertex_policy == VertexTransformPolicy::Quad2D)
        return PositionProviderId::VAB_Vec2;
    if (dim == MaterialRecipe::Dim::D2)
        return PositionProviderId::VAB_Vec2;
    return PositionProviderId::VAB_Vec3;
}

namespace
{
    static PassType PrimaryPassForBlendMode(const RenderAlphaMode blend) noexcept
    {
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

    struct RecipeAxisExpansion
    {
        VertexTransformPolicy vertex_policy = VertexTransformPolicy::Unknown;
        SurfaceShadingModel shading_model = SurfaceShadingModel::Unknown;
        MaterialResourceRequirements resources{};
        ShaderDataSchema schema = ShaderDataSchema::None;
    };

    static bool RowHasTextureMode(const MaterialVariantRow &row, const TextureSourceMode mode) noexcept
    {
        for (const auto &cs : row.color_sources)
        {
            if (mode == TextureSourceMode::Array)
            {
                if (cs.kind == graph::ColorSourceKind::BuiltinSampler2DArray)
                    return true;
            }
            else // Simple / Atlas / anything non-Array
            {
                if (cs.kind == graph::ColorSourceKind::BuiltinSampler2D)
                    return true;
            }
        }
        return false;
    }

    static RecipeAxisExpansion ExpandRecipeAxesFromPresetAlias(const MaterialRecipe &r) noexcept
    {
        RecipeAxisExpansion out{};

        const MaterialPreset resolved_preset =
            ResolveMaterialPresetForLOD(r.preset, GetDefaultMaterialLOD());

        const RenderAlphaMode target_blend = RenderAlphaMode::Opaque;
        const PassType target_pass = PrimaryPassForBlendMode(target_blend);
        const bool wants_array = HasAnyArrayTexture(r);

        const MaterialFeatureMask fmask = ResolveIntentFeatureMask(r.preset, r.intent_features);
        const LightingModel desired_lighting =
            ResolveLightingModelFromFeatures(fmask, LightingModel::Lambert);

        const MaterialVariantRow *best = nullptr;
        int best_score = -1000000;

        GetBuiltinVariantRegistry().ForEachBuiltinRow([&](const MaterialVariantRow &row)
        {
            if (row.preset != resolved_preset)
                return;

            int score = 0;
            if (row.blend == target_blend)
                score += 100;
            if (row.pass == target_pass)
                score += 50;

            if (row.resources.enable_lighting)
            {
                // lighting_model is ECS-injected via MaterialVariantKey; no per-row scoring needed.
                score += 40;
            }
            else if (desired_lighting == LightingModel::Lambert)
            {
                score += 5;
            }

            if (wants_array)
            {
                if (RowHasTextureMode(row, TextureSourceMode::Array))
                    score += 30;
            }
            else if (RowHasTextureMode(row, TextureSourceMode::Simple))
            {
                score += 20;
            }

            if (score > best_score)
            {
                best_score = score;
                best = &row;
            }
        });

        if (!best)
            return out;

        out.vertex_policy = best->vertex_policy;
        out.shading_model = best->surface_model;
        out.resources = best->resources;
        out.schema = best->schema;
        return out;
    }
}//namespace

/// Returns true if any color source in the recipe uses a Sampler2DArray (Array mode).
static bool HasAnyArrayTexture(const MaterialRecipe &r) noexcept
{
    for (const auto &cs : r.color_sources)
        if (cs.kind == graph::ColorSourceKind::BuiltinSampler2DArray)
            return true;
    return false;
}

static uint64 TryResolveBuiltinVariantRowHash(const MaterialPreset preset,
                                              const MaterialVariantKey &key) noexcept
{
    const MaterialPreset resolved_preset =
        ResolveMaterialPresetForLOD(preset, GetDefaultMaterialLOD());

    MaterialVariantKey query = key;
    query.variant_row_name_hash = 0;
    query.effective_feature_mask = 0;

    GetBuiltinVariantRegistry().ForEach(
        [&](const MaterialVariantKey &candidate_key, const MaterialVariantDesc &desc)
        {
            if (candidate_key.variant_row_name_hash == 0)
                return;
            if (!desc.factory_type.has_value() || ResolveMaterialPresetForLOD(*desc.factory_type, GetDefaultMaterialLOD()) != resolved_preset)
                return;

            MaterialVariantKey candidate = candidate_key;
            const uint64 candidate_row_hash = candidate.variant_row_name_hash;
            candidate.variant_row_name_hash = 0;
            candidate.effective_feature_mask = 0;

            MaterialVariantKey local_query = query;
            // sky_ambient_model is never a routing axis: always canonicalize to Simple.
            local_query.sky_ambient_model = SkyLightAmbientModel::Simple;

            if (candidate == local_query)
            {
                query.variant_row_name_hash = candidate_row_hash;
            }
        });

    if (query.variant_row_name_hash != 0)
        return query.variant_row_name_hash;

    return key.variant_row_name_hash;
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

static StaticMaterialDefId GetPureColorDefId(const MaterialRecipe &r)
{
    const bool is2d = (r.dim == MaterialRecipe::Dim::D2);

    static const FixedVertexEntry kVertex2D[] = {
        { VAT_VEC2, VAN::Position },
    };
    static const FixedVertexEntry kVertex3D[] = {
        { VAT_VEC3, VAN::Position },
    };

    const FixedVertexEntry *vertex = is2d ? kVertex2D : kVertex3D;
    const uint32_t vertex_count = 1;

    const char *def_name = is2d ? "PureColor2D_v1" : "PureColor3D_v1";

    const StaticMaterialDef def {
        def_name,
        r.prim,
        vertex,
        vertex_count,
        nullptr,
        nullptr,
        nullptr,
        ShaderDataSchema::Color4f,
    };

    return AcquireStaticMaterialDefId(def);
}

static StaticMaterialDefId GetGizmo3DDefId()
{
    static const FixedVertexEntry kVertex[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_VEC3, VAN::Normal },
    };
    static const UBOSemanticSet kUBOs = {
        UBODescriptorSemantic::ViewportInfo,
        UBODescriptorSemantic::CameraInfo,
    };
    static const SSBOSemanticSet kSSBOs = {
        SSBODescriptorSemantic::TransformData,
        SSBODescriptorSemantic::TransformID,
        SSBODescriptorSemantic::MaterialBindingInstanceID,
        SSBODescriptorSemantic::MaterialBindingInstanceData,
    };

    static StaticMaterialDefId s_id = kInvalidStaticMaterialDefId;
    if (s_id == kInvalidStaticMaterialDefId)
    {
        const StaticMaterialDef def {
            "Gizmo3D_v1",
            PrimitiveType::Triangles,
            kVertex,
            uint32_t(sizeof(kVertex) / sizeof(kVertex[0])),
            &kUBOs,
            &kSSBOs,
            nullptr,
            ShaderDataSchema::Color4f,
        };
        s_id = AcquireStaticMaterialDefId(def);
    }

    return s_id;
}

/// Resolve the StaticMaterialDefId for the given recipe.
/// Returns kInvalidStaticMaterialDefId for presets that have no registered def.
static StaticMaterialDefId ResolveDefIdForRecipe(const MaterialRecipe &r) noexcept
{
    switch (r.preset)
    {
    case MaterialPreset::PureColor:
        return GetPureColorDefId(r);

    case MaterialPreset::Gizmo3D:
        return GetGizmo3DDefId();

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
    case MaterialPreset::PureColor:
    case MaterialPreset::Gizmo3D:
        return ShaderDataSchema::Color4f;

    case MaterialPreset::Text2D:
        return ShaderDataSchema::TextColor;

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
    // Validate: user PCG provider path is only legal when preset == Custom.
    // A non-Custom preset has a builtin variant row that owns position_provider;
    // allowing a recipe to silently override it would create silent routing ambiguity.
    assert((r.user_provider_path.empty() || r.preset == MaterialPreset::Custom)
           && "user_provider_path is only valid when preset == MaterialPreset::Custom");

    std::printf("[RecipeToKey] BuildBaseVariantKeyFromRecipe enter: preset=%u dim=%u prim=%u\n",
                static_cast<unsigned>(r.preset),
                static_cast<unsigned>(r.dim),
                static_cast<unsigned>(r.prim));

    const RecipeAxisExpansion alias_axes = ExpandRecipeAxesFromPresetAlias(r);

    // ── Step 1: preset → base variant key ────────────────────────────────────
    // Routes through RouteKey() with two targeted hints:
    //   • ov.position_provider: soft preference (VAB_Vec2 for 2D, VAB_Vec3 for 3D).
    //     Selects the correct variant when a preset has both a 2D entry (VAB_Vec2 position
    //     attribute) and a 3D entry (VAB_Vec3). Presets whose 2D/3D shader is identical
    //     (e.g. PureColor, which has no position vertex attribute) have only one entry and
    //     are found on the fallback pass regardless of the hint.
    //   • ov.preferred_vertex_policy: hard filter, used only for billboard presets whose
    //     VS transform logic genuinely differs by vertex policy (BillboardCameraFacing, etc.).
    MaterialVariantKey k;
    {
        RuntimeKeyOverrides rov{};

        const VertexTransformPolicy effective_vertex_policy =
            (r.vertex_policy != VertexTransformPolicy::Unknown)
            ? r.vertex_policy
            : alias_axes.vertex_policy;

        // Billboard vertex policies need a hard filter: their VS transform is different.
        if (effective_vertex_policy == VertexTransformPolicy::BillboardCameraFacing)
            rov.preferred_vertex_policy = VertexTransformPolicy::BillboardCameraFacing;
        else if (effective_vertex_policy == VertexTransformPolicy::BillboardAxisLocked)
            rov.preferred_vertex_policy = VertexTransformPolicy::BillboardAxisLocked;

        // Do NOT preset position_provider in rov here; let RouteKey pick up the builtin
        // entry's native position_provider first (e.g. PCG_FullscreenTriangle).
        // We apply the dim-based override AFTER RouteKey, gated by manifest allow_dim_override,
        // so that PCG providers whose manifest declares allow_dim_override=false are preserved.
        k = RouteKey(r.preset, 0u, rov);

        std::printf("[RecipeToKey] Phase6: after RouteKey pos_provider=%u (0=Unk,2=Vec2,3=Vec3) dim=%u policy=%u\n",
                    static_cast<unsigned>(k.position_provider),
                    static_cast<unsigned>(r.dim),
                    static_cast<unsigned>(effective_vertex_policy));

        // Phase 6: use manifest allow_dim_override instead of IsPCGPositionProvider().
        // If the manifest is absent (unregistered ID) we conservatively treat it as
        // non-overridable (same safety as old PCG guard).
        {
            const hgl::graph::ProviderManifest* pm =
                hgl::graph::ProviderManifestRegistry::FindByPosId(k.position_provider);
            const bool may_override = pm ? pm->allow_dim_override : false;

            std::printf("[RecipeToKey] Phase6: manifest=%p allow_dim_override=%d manifest_count=%u\n",
                        (const void*)pm,
                        (int)may_override,
                        (unsigned)hgl::graph::ProviderManifestRegistry::Count());

            if (may_override)
            {
                const PositionProviderId new_provider =
                    ResolveProviderFromDemand(r.preset, r.dim, effective_vertex_policy);
                std::printf("[RecipeToKey] Phase6: override pos_provider %u -> %u\n",
                            static_cast<unsigned>(k.position_provider),
                            static_cast<unsigned>(new_provider));
                k.position_provider = new_provider;
            }
        }
    }

    // ── UserPCG: wire user_provider_path_hash ─────────────────────────────────
    // Must happen after RouteKey so that k.position_provider is already set from
    // the builtin row (for Custom preset it will be Unknown/default; we overwrite
    // it here to UserPCG and record the path hash for the compositor).
    {
        if (!r.user_provider_path.empty())
        {
            const hgl::graph::ProviderManifest* pm =
                hgl::graph::ProviderManifestRegistry::AcquireUserProvider(r.user_provider_path);
            k.position_provider       = PositionProviderId::UserPCG;
            k.user_provider_path_hash = pm ? pm->glsl_path_hash : hgl::graph::Fnv1a32(r.user_provider_path);
        }
        else if (r.position_provider != hgl::graph::PositionProviderId::Unknown)
        {
            // Explicit position_provider set on recipe (non-PCG path, e.g. VAB_Vec3)
            k.position_provider = r.position_provider;
        }
    }

    // ── Step 2: dimension ─────────────────────────────────────────────────────
    // vertex_policy and position_provider are already correctly set by the matched
    // builtin entry via RouteKey (Step 1). Dimension is a config-layer concern; only
    // entries that truly differ in shader behaviour (e.g. UnlitTexture2D vs UnlitTexture)
    // have separate builtin rows. Do NOT re-encode dim here.

    // ── Step 3: position provider ─────────────────────────────────────────────
    // 2D recipes always use VAB_Vec2; 3D recipes use VAB_Vec3.
    // No recipe-level override needed now that pos_format is removed.

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

    // ── Step 5: texture source-mode overrides (from color_sources) ─────────────
    // Derive per-slot TextureSourceMode from color_sources[] for key construction.
    for (const auto &cs : r.color_sources)
    {
        if (cs.slot == graph::mtl::SamplerSlot::RANGE_SIZE)
            continue;
        if (cs.kind == graph::ColorSourceKind::BuiltinSampler2D)
            k.SetTextureSourceMode(cs.slot, TextureSourceMode::Simple);
        else if (cs.kind == graph::ColorSourceKind::BuiltinSampler2DArray)
            k.SetTextureSourceMode(cs.slot, TextureSourceMode::Array);
    }

    // ── Step 6: render_phase / quality_level from GlobalRenderConfig ────────
    // VA bits are now determined by Matcher via SFM annotations at match time.
    // RecipeToKey only records the cache-key axes that are globally stable.
    {
        const auto& cfg = hgl::graph::mtl::GlobalRenderConfig::Instance();
        k.render_phase   = hgl::graph::mtl::RenderPhase::ForwardOpaque; // default; caller overrides for shadow/EarlyZ
        k.quality_level  = static_cast<uint8>(cfg.GetQualityLevel());
    }

    // ── Step 7: blend_mode + pass_hint (Phase A) ──────────────────────────────
    {
        const RenderAlphaMode blend = r.default_render_state.blend;
        k.blend_mode = blend;
        k.pass_hint  = GetPrimaryPassForBlendMode(blend);
    }

    return k;
}

MaterialVariantKey ApplyRouterCanonicalization(const MaterialVariantKey &in) noexcept
{
    MaterialVariantKey k = in;

    // sky_ambient_model is never a routing axis: always canonicalize to Simple.
    k.sky_ambient_model = SkyLightAmbientModel::Simple;

    return k;
}

} // namespace detail

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

MaterialKey ResolveRecipePrimaryKey(const MaterialRecipe &r) noexcept
{
    MaterialKey k{};

    const RecipeAxisExpansion alias_axes = ExpandRecipeAxesFromPresetAlias(r);

    // Phase A: build un-canonicalized variant key
    MaterialVariantKey vk = detail::BuildBaseVariantKeyFromRecipe(r);

    // Phase B: apply router canonicalization
    vk = detail::ApplyRouterCanonicalization(vk);
    vk.variant_row_name_hash = TryResolveBuiltinVariantRowHash(r.preset, vk);
    k.variant = vk;

    // Phase C: primary pass
    k.pass = detail::GetPrimaryPassForBlendMode(vk.blend_mode);

    // Phase D: schema
    if (r.has_explicit_schema && r.schema != ShaderDataSchema::None)
        k.schema = r.schema;
    else if (alias_axes.schema != ShaderDataSchema::None)
        k.schema = alias_axes.schema;
    else
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
