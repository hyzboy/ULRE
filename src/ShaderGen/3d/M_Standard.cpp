#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <cstdio>
#include <vector>
#include <memory>

#include <hgl/mtl/MaterialVariantDesc.h>
#include <hgl/mtl/SamplerSlot.h>

#include "MaterialFactory3DCommon.h"
#include "StandardDescriptorBuilder.h"
#include "StandardStaticDef.h"
#include "StandardVariantRouter.h"

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

    static constexpr const char *kAttribFetchMacroTags[] = {
        "NORMAL",            // 0
        "TANGENT",           // 1
        "COLOR",             // 2
        "TEXCOORD0",         // 3
        "TEXCOORD1",         // 4
        "JOINTS",            // 5
        "WEIGHTS",           // 6
        "INSTANCETRANSFORM", // 7
    };
    static_assert(
        sizeof(kAttribFetchMacroTags) / sizeof(*kAttribFetchMacroTags)
            == size_t(AttributeSemantic::BuiltinCount),
        "kAttribFetchMacroTags size mismatch");

} // anonymous namespace

std::unique_ptr<MaterialCreateInfo> CreateStandardVariantOwned(const contract::PhysicalDeviceProfileLite *profile,
                                                               const MaterialVariantDesc &desc,
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

    uint32_t standard_tex_slot_count = 0;
    const SamplerSlot *standard_tex_slots = GetStandardTextureSlots(standard_tex_slot_count);

    if (standard_tex_slot_count != 2)
    {
        std::fprintf(stderr,
            "[Standard] CreateStandardVariant failed: unexpected texture slot count=%u\n",
            standard_tex_slot_count);
        return nullptr;
    }

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
        standard_tex_slots,
        standard_tex_slot_modes,
        standard_tex_slot_count,
        policy.any_array,
        GetStandardBaseSSBOs(),
        cfg_with_mi,
        ambient,
        lighting,
        dynamic_ssbos,
        dynamic_samplers,
        unused_resources,
        any_array);

    StaticMaterialDef dynamic_def = BuildStandardDynamicDef(
        BuildCanonicalStandardStaticDef(any_array),
        dynamic_ssbos,
        dynamic_samplers,
        ShaderDataSchema::StandardParams,
        any_array);

    MaterialVariantKey route_key = policy.route_key;
    route_key.lighting_model = lighting;
    // Registry descriptors are not split by sky model; keep lookup key on canonical sky.
    route_key.sky_ambient_model = SkyLightAmbientModel::Simple;

    const MaterialVariantDesc *var_desc = &desc;
    PrintStandardRouteKey("VariantRegistry resolved route-request", route_key, any_array);
    PrintStandardRouteKey("VariantRegistry resolved route-final", route_key, any_array);
    if (kStandardVerbose)
    {
        std::fprintf(stderr,
            "[Standard] VariantRegistry resolved variant=%s\n",
            var_desc->variant_name.c_str());
    }

    // Populate vertex attribute feature bits from the actual vertex layout.
    // policy is const, so take a mutable copy of assemble_key.
    MaterialVariantKey assemble_key = policy.assemble_key;
    assemble_key.lighting_model = lighting;
    assemble_key.sky_ambient_model = ambient;
    PopulateVariantKeyVertexAttribBits(assemble_key, dynamic_def);

    // If the assembled key uses any SSBO-backed vertex streams, register them
    // in the VertexStreams descriptor set so the pipeline layout includes set=4.
    dynamic_def.vertex_stream_key = &assemble_key;

    CompositorAssembler assembler;

    auto result = assembler.Assemble(assemble_key, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[Standard] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    auto mci = CompileCompositorMaterialOwned(
        profile,
        dynamic_def,
        result.vertex_glsl,
        result.fragment_glsl,
        &cfg_with_mi);

    if (!mci)
        std::fprintf(stderr, "[Standard] CompileCompositorMaterial failed\n");
    return mci;
}

static std::unique_ptr<MaterialCreateInfo> Standard_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *desc,
    const MaterialVariantKey                  &key,
    MaterialCreateConfig *cfg)
{ return CreateStandardVariantOwned(profile, *desc, key, static_cast<const Material3DCreateConfig *>(cfg)); }

}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(Standard, "Standard", hgl::graph::mtl::Standard_Adapter)

