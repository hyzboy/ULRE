#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <cstdio>
#include <vector>

#include <hgl/mtl/MaterialVariantRegistry.h>
#include <hgl/mtl/SamplerSlot.h>

#include "StandardDescriptorBuilder.h"
#include "StandardVariantRouter.h"

namespace hgl::graph::mtl{
namespace
{
    // MI layout — same whether slots are sampler2D or sampler2DArray.
    // Layer indices for Array slots are stored separately in the MIT SSBO (MaterialInstanceTextureID).
    constexpr VertexAttributeSpec STANDARD_VERTEX_SPECS[] = {
        { VAN::Position, VAT_VEC3, PF_RGB32F },
        { VAN::TexCoord, VAT_VEC2, PF_RG32F  },
        { VAN::Normal,   VAT_VEC3, PF_RGB32F },
    };

    // Non-texture descriptors only �?texture entries are built dynamically in CreateStandardVariant().
    const UBOSemanticSet STANDARD_BASE_UBOS = {
        UBODescriptorSemantic::ViewportInfo,
        UBODescriptorSemantic::CameraInfo,
        UBODescriptorSemantic::SkyInfo,
    };

    const SSBOSemanticSet STANDARD_BASE_SSBOS = {
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

    const StaticMaterialDef STANDARD_DEF_TEMPLATE {
        "Standard_v1",
        PrimitiveType::Triangles,
        nullptr,
        0,
        &STANDARD_BASE_UBOS,
        &STANDARD_BASE_SSBOS,
        nullptr,
        nullptr, 0,
        InstanceDataLayout::PBRStandard,
        STANDARD_VERTEX_SPECS,
        uint32_t(sizeof(STANDARD_VERTEX_SPECS) / sizeof(STANDARD_VERTEX_SPECS[0])),
    };

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

    const StandardVariantPolicyResult policy = BuildStandardVariantPolicy(input_key);

    const TextureSourceMode standard_tex_slot_modes[] = {
        policy.resolved_base,
        policy.resolved_normal,
    };

    Material3DCreateConfig cfg_with_mi;
    SkyLightAmbientModel ambient = SkyLightAmbientModel::Simple;
    LightingModel lighting = LightingModel::Lambert;

    // Start with stable non-texture descriptors, then append texture entries.
    SSBOSemanticSet dynamic_ssbos;
    StaticTextureSamplerDescriptors dynamic_samplers;
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

    StaticMaterialDef dynamic_def = BuildStandardDynamicDef(
        STANDARD_DEF_TEMPLATE,
        dynamic_ssbos,
        dynamic_samplers,
        InstanceDataLayout::PBRStandard,
        any_array);

    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(policy.route_key);
    if (!var_desc)
    {
        std::fprintf(stderr,
            "[Standard] VariantRegistry lookup failed (route_hash=%llu surface=%u geom=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X any_array=%d)\n",
            static_cast<unsigned long long>(policy.route_key.Hash()),
            static_cast<unsigned>(policy.route_key.GetSurfaceType()),
            static_cast<unsigned>(policy.route_key.geometry_mode),
            policy.route_key.texture_source_bits,
            policy.route_key.sampler_feature_bits,
            policy.route_key.vertex_attribute_feature_bits,
            policy.route_key.extra_feature_bits,
            any_array ? 1 : 0);
        return nullptr;
    }

    CompositorAssembler assembler;

    auto result = assembler.Assemble(policy.assemble_key, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[Standard] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
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
    key.SetSurfaceType(SurfaceType::Standard);
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

