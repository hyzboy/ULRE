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
#include <hgl/mtl/MaterialVariantRow.h> // VertexTransformPolicy, VertexInputProfile, SurfaceShadingModel
#include <hgl/mtl/PassExpansion.h>
#include <hgl/mtl/MaterialKeyToolchainVersion.h>
#include <hgl/mtl/StaticMaterialDefRegistry.h>
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/mtl/MaterialVariantRegistry.h>
#include <hgl/shadergen/ColorSource.h>

namespace hgl::graph::mtl
{

// ─────────────────────────────────────────────────────────────────────────────
// File-local helpers (not exposed in the header)
// ─────────────────────────────────────────────────────────────────────────────

static bool HasAnyArrayTexture(const MaterialRecipe &r) noexcept;

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
    VertexInputProfile vertex_input = VertexInputProfile::Unknown;
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
            if (row.resources.lighting_model == desired_lighting)
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

    out.vertex_input = best->vertex_input;
    out.vertex_policy = best->vertex_policy;
    out.shading_model = best->surface_model;
    out.resources = best->resources;
    out.schema = best->schema;
    return out;
}
}

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
            if (!desc.bound_row || !desc.bound_row->sky_is_routing_axis)
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
    const RecipeAxisExpansion alias_axes = ExpandRecipeAxesFromPresetAlias(r);

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

    // ── Step 3: position provider ─────────────────────────────────────────────
    // Mirrors fixed-def key population: vec2 position inputs must route as VAB_Vec2.
    const VertexInputProfile effective_vertex_input =
        (r.vertex_input != VertexInputProfile::Unknown)
        ? r.vertex_input
        : alias_axes.vertex_input;

    switch (effective_vertex_input)
    {
    case VertexInputProfile::Position2D:
    case VertexInputProfile::PositionLuminance2D:
    case VertexInputProfile::PositionTexCoord2D:
        k.position_provider = PositionProviderId::VAB_Vec2;
        break;
    default:
        break;
    }

    // Mirrors Material2DCreateConfig::position_format in ApplyCreateConfigToVariantKey.
    if (r.pos_format.Check())
    {
        k.position_provider = (r.pos_format.vec_size == 2)
            ? PositionProviderId::VAB_Vec2
            : PositionProviderId::DirectVec3;
    }

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

    // ── Step 8: explicit axis overrides (Phase B) ─────────────────────────────
    // Explicit value has priority; Unknown falls back to preset alias expansion.
    const VertexTransformPolicy effective_vertex_policy =
        (r.vertex_policy != VertexTransformPolicy::Unknown)
        ? r.vertex_policy
        : alias_axes.vertex_policy;

    if (effective_vertex_policy != VertexTransformPolicy::Unknown)
    {
        switch (effective_vertex_policy)
        {
        case VertexTransformPolicy::Mesh3D:
            k.geometry_mode = GeometryMode::Mesh3D; break;
        case VertexTransformPolicy::Quad2D:
            k.geometry_mode = GeometryMode::Quad2D; break;
        case VertexTransformPolicy::BillboardCameraFacing:
            k.geometry_mode = GeometryMode::BillboardCameraFacing; break;
        case VertexTransformPolicy::BillboardAxisLocked:
            k.geometry_mode = GeometryMode::BillboardAxisLocked; break;
        default:
            // Sky, TerrainGrid, Text2D, FullscreenTriangle etc. have no
            // GeometryMode counterpart yet; leave geometry_mode unchanged.
            break;
        }
    }

    return k;
}

/// Phase 6 (replaces Phase 1 TODO bridge): table-driven sky canonicalization.
/// Scans the builtin entry table for any entry whose key (with sky set to Simple)
/// matches the incoming key. If the matched entry has sky_is_routing_axis==false,
/// canonicalize sky to Simple. This is the same rule used by IsSkyRoutingAxisForPresetKey
/// in MaterialLibrary.cpp, expressed here without a known preset parameter.
static bool SkyIsRoutingAxisForKey(const MaterialVariantKey &key) noexcept
{
    // Build a probe key with sky canonicalized so we can match entries regardless
    // of the current sky value.
    MaterialVariantKey probe = key;
    probe.variant_row_name_hash = 0;
    probe.effective_feature_mask = 0;
    probe.sky_ambient_model = SkyLightAmbientModel::Simple;

    bool found = false;
    bool routing_axis = false;
    GetBuiltinVariantRegistry().ForEach(
        [&](const MaterialVariantKey &candidate_key, const MaterialVariantDesc &desc)
        {
            if (found)
                return;

            MaterialVariantKey candidate = candidate_key;
            candidate.variant_row_name_hash = 0;
            candidate.effective_feature_mask = 0;
            if (candidate == probe)
            {
                found = true;
                routing_axis = desc.bound_row ? desc.bound_row->sky_is_routing_axis : false;
            }
        });

    if (found)
        return routing_axis;

    // No row matched; conservatively treat sky as NOT a routing axis.
    return false;
}

MaterialVariantKey ApplyRouterCanonicalization(const MaterialVariantKey &in) noexcept
{
    MaterialVariantKey k = in;

    // Phase 6: table-driven sky routing-axis check replaces the Phase 1
    // hardcoded "Standard && Mesh3D → canonicalize sky to Simple" branch.
    // The rule is now: canonicalize sky_ambient_model to Simple unless the
    // matching builtin row explicitly declares sky_is_routing_axis = true.
    if (!SkyIsRoutingAxisForKey(k))
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
