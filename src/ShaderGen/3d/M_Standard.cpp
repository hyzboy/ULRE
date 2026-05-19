#include "MaterialFactory3DCommon.h"
#include "StandardDescriptorBuilder.h"
#include "StandardVariantRouter.h"
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/SamplerSlot.h>
#include <cstdio>
#include <vector>

namespace hgl::graph::mtl{
namespace
{
#if defined(ULRE_SHADERGEN_VERBOSE)
    constexpr bool kStandardVerbose = true;
#else
    constexpr bool kStandardVerbose = false;
#endif

    static void PrintStandardRouteKey(const char *label, const MaterialVariantKey &key, const bool any_array)
    {
        if (!kStandardVerbose)
            return;

        std::fprintf(stderr,
            "[Standard] %s hash=%llu surface=%u geom=%u sky=%u light=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X any_array=%d\n",
            label ? label : "route",
            static_cast<unsigned long long>(key.Hash()),
            static_cast<unsigned>(key.surface_type),
            static_cast<unsigned>(key.geometry_mode),
            static_cast<unsigned>(key.sky_ambient_model),
            static_cast<unsigned>(key.lighting_model),
            key.texture_source_bits,
            key.sampler_feature_bits,
            key.vertex_attribute_feature_bits,
            key.extra_feature_bits,
            any_array ? 1 : 0);
    }

    constexpr FixedVertexEntry STANDARD_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_VEC2, VAN::TexCoord },
        { VAT_VEC3, VAN::Normal },
    };

    // Non-texture descriptors; texture entries are built dynamically in CreateStandardVariant().
    const SSBOSemanticSet STANDARD_BASE_SSBOS = {
        SSBODescriptorSemantic::TransformData,
        SSBODescriptorSemantic::TransformID,
        SSBODescriptorSemantic::MaterialBindingInstanceID,
        SSBODescriptorSemantic::MaterialBindingInstanceData,
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
        STANDARD_VERTEX,
        uint32_t(sizeof(STANDARD_VERTEX) / sizeof(STANDARD_VERTEX[0])),
        nullptr,
        &STANDARD_BASE_SSBOS,
        nullptr,
        ShaderDataSchema::StandardParams,
    };

    static MaterialCreateInfo *CreateStandardFactory(
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialVariantDesc                 *desc,
        const MaterialVariantKey                  &input_key,
        MaterialCreateConfig                      *cfg)
    {
        auto *cfg_3d=static_cast<const Material3DCreateConfig *>(cfg);
        if (!cfg_3d)
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

        SSBOSemanticSet dynamic_ssbos;
        StaticTextureSamplerDescriptors dynamic_samplers;
        std::vector<const char *> unused_resources;
        bool any_array = false;

        BuildStandardDescriptorState(
            cfg_3d,
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
            ShaderDataSchema::StandardParams,
            any_array);

        MaterialVariantKey route_key = policy.route_key;
        route_key.lighting_model = lighting;
        route_key.sky_ambient_model = SkyLightAmbientModel::Simple;

        PrintStandardRouteKey("VariantRegistry resolved route-request", route_key, any_array);
        PrintStandardRouteKey("VariantRegistry resolved route-final", route_key, any_array);
        if (kStandardVerbose)
        {
            std::fprintf(stderr,
                "[Standard] VariantRegistry resolved variant=%s\n",
                desc->variant_name.c_str());
        }

        MaterialVariantKey assemble_key = policy.assemble_key;
        assemble_key.lighting_model = lighting;
        assemble_key.sky_ambient_model = ambient;
        PopulateVariantKeyVertexAttribBits(assemble_key, dynamic_def);

        CompositorAssembler assembler;

        auto result = assembler.Assemble(assemble_key, *desc);

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

} // anonymous namespace

}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(Standard, "Standard", hgl::graph::mtl::CreateStandardFactory)

