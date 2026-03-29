#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/shadergen/ShaderRequireScanner.h>
#include <hgl/shadergen/ShaderGenPathConfig.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <cstdio>
#include <vector>

#include <hgl/mtl/MaterialVariantDesc.h>
#include <hgl/mtl/SamplerName.h>

#include "StandardDescriptorBuilder.h"
#include "StandardProfileAdapter.h"
#include "StandardVariantPolicy.h"

#ifndef ULRE_STANDARD_USE_PROFILE_ADAPTER
#define ULRE_STANDARD_USE_PROFILE_ADAPTER 1
#endif

#ifndef ULRE_STANDARD_POLICY_EQUIV_CHECK
#define ULRE_STANDARD_POLICY_EQUIV_CHECK 1
#endif

namespace hgl::graph::mtl{
namespace
{
    // MI layout — same whether slots are sampler2D or sampler2DArray.
    // Layer indices for Array slots are stored separately in the MIT SSBO (MaterialInstanceTextureID).
    constexpr const char mi_codes_simple[] = R"(
        uint  base_color;
        float metallic;
        float roughness;
        float normal_scale;
    )";
    constexpr const uint32_t mi_bytes_simple = sizeof(uint32_t) + sizeof(float) * 3;

    constexpr FixedVertexEntry STANDARD_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_VEC2, VAN::TexCoord },
        { VAT_VEC3, VAN::Normal },
    };

    // Non-texture descriptors only �?texture entries are built dynamically in CreateStandardVariant().
    const FixedUBODescriptors STANDARD_BASE_UBOS = {
        UBODescriptorSemantic::ViewportInfo,
        UBODescriptorSemantic::CameraInfo,
        UBODescriptorSemantic::SkyInfo,
    };

    const FixedSSBODescriptors STANDARD_BASE_SSBOS = {
        SSBODescriptorSemantic::TransformData,
        SSBODescriptorSemantic::TransformID,
        SSBODescriptorSemantic::MaterialInstanceID,
        SSBODescriptorSemantic::MaterialInstanceData,
    };

    // Ordered list of texture slots used by the Standard material.
    // Standard is a schema-fixed material: extending slots (e.g. Emissive/ORM) means a new material type,
    // not an in-place quality/feature variant inside Standard.
    constexpr SamplerSlot STANDARD_TEX_SLOTS[] = {
        SamplerSlot::BaseColor,
        SamplerSlot::Normal,
    };
    constexpr uint32_t STANDARD_TEX_SLOT_COUNT = uint32_t(sizeof(STANDARD_TEX_SLOTS) / sizeof(STANDARD_TEX_SLOTS[0]));
    static_assert(STANDARD_TEX_SLOT_COUNT == 2, "Standard material slot schema is fixed (BaseColor + Normal).");

    const FixedMaterialDef STANDARD_DEF_TEMPLATE {
        "Standard_v1",
        PrimitiveType::Triangles,
        STANDARD_VERTEX,
        uint32_t(sizeof(STANDARD_VERTEX) / sizeof(STANDARD_VERTEX[0])),
        &STANDARD_BASE_UBOS,
        &STANDARD_BASE_SSBOS,
        nullptr,
        mi_codes_simple,
        mi_bytes_simple,
    };

    bool IsSameStandardPolicy(const StandardVariantPolicyResult &lhs,
                              const StandardVariantPolicyResult &rhs)
    {
        return lhs.resolved_base == rhs.resolved_base
            && lhs.resolved_normal == rhs.resolved_normal
            && lhs.base_is_array == rhs.base_is_array
            && lhs.normal_is_array == rhs.normal_is_array
            && lhs.any_array == rhs.any_array
            && lhs.route_key.Hash() == rhs.route_key.Hash()
            && lhs.assemble_key.Hash() == rhs.assemble_key.Hash();
    }

} // anonymous namespace

MaterialCreateInfo *CreateStandardVariant(const contract::PhysicalDeviceProfileLite *profile,
                                          const MaterialVariantKey &input_key,
                                          const Material3DCreateConfig *cfg)
{
    if (!cfg)
    {
        std::fprintf(stderr, "[Standard] CreateStandardVariant failed: cfg is null\n");
        return nullptr;
    }

    if (!profile)
    {
        std::fprintf(stderr, "[Standard] CreateStandardVariant warning: profile is null\n");
    }

    const StandardVariantPolicyResult direct_policy = BuildStandardVariantPolicy(input_key);

    StandardVariantPolicyResult adapter_policy{};
    bool adapter_ok = false;
    std::vector<std::string> profile_diagnostics;

#if ULRE_STANDARD_USE_PROFILE_ADAPTER || ULRE_STANDARD_POLICY_EQUIV_CHECK
    {
        const MaterialProfileAsset profile_asset = BuildBuiltinStandardDefaultProfile();
        adapter_ok = BuildStandardPolicyFromProfile(profile_asset,
                                                    input_key,
                                                    adapter_policy,
                                                    profile_diagnostics);

#if ULRE_STANDARD_USE_PROFILE_ADAPTER
        if (!adapter_ok)
        {
            std::fprintf(stderr, "[Standard] BuildStandardPolicyFromProfile failed\n");
            for (const auto &msg : profile_diagnostics)
                std::fprintf(stderr, "[Standard] %s\n", msg.c_str());
            return nullptr;
        }
#endif
    }
#endif

#if ULRE_STANDARD_POLICY_EQUIV_CHECK
    if (adapter_ok && !IsSameStandardPolicy(direct_policy, adapter_policy))
    {
        std::fprintf(stderr,
                     "[Standard] Policy equivalence mismatch direct(route=%llu assemble=%llu any_array=%d) adapter(route=%llu assemble=%llu any_array=%d)\n",
                     static_cast<unsigned long long>(direct_policy.route_key.Hash()),
                     static_cast<unsigned long long>(direct_policy.assemble_key.Hash()),
                     direct_policy.any_array ? 1 : 0,
                     static_cast<unsigned long long>(adapter_policy.route_key.Hash()),
                     static_cast<unsigned long long>(adapter_policy.assemble_key.Hash()),
                     adapter_policy.any_array ? 1 : 0);
    }
#endif

#if ULRE_STANDARD_USE_PROFILE_ADAPTER
    const StandardVariantPolicyResult &policy = adapter_policy;
#else
    const StandardVariantPolicyResult &policy = direct_policy;
#endif

    const TextureSourceMode standard_tex_slot_modes[] = {
        policy.resolved_base,
        policy.resolved_normal,
    };

    Material3DCreateConfig cfg_with_mi;
    SkyLightAmbientModel ambient = SkyLightAmbientModel::Simple;
    LightingModel lighting = LightingModel::Lambert;

    // Start with stable non-texture descriptors, then append texture entries.
    FixedSSBODescriptors dynamic_ssbos;
    FixedTextureSamplerDescriptors dynamic_samplers;
    std::vector<const char *> unused_resources;
    bool any_array = false;

    BuildStandardDescriptorState(
        cfg,
        STANDARD_TEX_SLOTS,
        standard_tex_slot_modes,
        STANDARD_TEX_SLOT_COUNT,
        policy.any_array,
        STANDARD_BASE_SSBOS,
        cfg_with_mi,
        ambient,
        lighting,
        dynamic_ssbos,
        dynamic_samplers,
        unused_resources,
        any_array);

    FixedMaterialDef dynamic_def = BuildStandardDynamicDef(
        STANDARD_DEF_TEMPLATE,
        dynamic_ssbos,
        dynamic_samplers,
        mi_codes_simple,
        mi_bytes_simple,
        any_array);

    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(policy.route_key);
    if (!var_desc)
    {
        std::fprintf(stderr,
            "[Standard] VariantRegistry lookup failed (route_hash=%llu surface=%u geom=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X any_array=%d)\n",
            static_cast<unsigned long long>(policy.route_key.Hash()),
            static_cast<unsigned>(policy.route_key.surface_type),
            static_cast<unsigned>(policy.route_key.geometry_mode),
            policy.route_key.texture_source_bits,
            policy.route_key.sampler_feature_bits,
            policy.route_key.vertex_attribute_feature_bits,
            policy.route_key.extra_feature_bits,
            any_array ? 1 : 0);
        return nullptr;
    }

    CompositorAssembler assembler;

    auto result = assembler.Assemble(policy.assemble_key, *var_desc, ambient, lighting);

    if (!result.success)
    {
        std::fprintf(stderr, "[Standard] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    ShaderAutoRequirements auto_requirements;
    std::string require_diagnostics;
    const bool require_ok = CollectShaderAutoRequirements(dynamic_def,
                                                          GetShaderLibraryPath(),
                                                          result.vertex_glsl,
                                                          result.fragment_glsl,
                                                          auto_requirements,
                                                          &require_diagnostics);
    if (!require_ok)
    {
        std::fprintf(stderr, "[Standard] reflection collection failed:\n%s", require_diagnostics.c_str());
        return nullptr;
    }

    FixedUBODescriptors merged_ubos;
    FixedSSBODescriptors merged_ssbos;
    FixedTextureSamplerDescriptors merged_samplers;
    FixedMaterialDef merged_def = dynamic_def;
    MergeShaderAutoRequirements(dynamic_def,
                                auto_requirements,
                                merged_def,
                                merged_ubos,
                                merged_ssbos,
                                merged_samplers);

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        merged_def,
        result.vertex_glsl,
        result.fragment_glsl,
        &cfg_with_mi);

    if (!mci)
        std::fprintf(stderr, "[Standard] CompileCompositorMaterial failed\n");
    return mci;
}

// Unified factory  TextureSourceMode::Simple  -> sampler2D  (classic single-texture Standard)
//                  TextureSourceMode::Array   -> sampler2DArray (texture-atlas / array Standard)
MaterialCreateInfo *CreateStandard(const contract::PhysicalDeviceProfileLite *profile,
                                   const Material3DCreateConfig *cfg,
                                   TextureSourceMode tex_source)
{
    MaterialVariantKey key;
    key.surface_type = SurfaceType::Standard;
    key.SetTextureSourceMode(SamplerSlot::BaseColor, tex_source);
            key.SetTextureSourceMode(SamplerSlot::Normal, tex_source);
    return CreateStandardVariant(profile, key, cfg);
}

// Compat wrappers keep the two named entry-points so MaterialLibrary.cpp
// does not need to change its dispatch table.
MaterialCreateInfo *CreateStandard(const contract::PhysicalDeviceProfileLite *profile,
                                   const Material3DCreateConfig *cfg)
{
    return CreateStandard(profile, cfg, TextureSourceMode::Simple);
}

}//namespace hgl::graph::mtl

