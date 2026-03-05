#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/graph/mtl/UBOCommon.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/graph/mtl/Material3DCreateConfig.h>
#include <hgl/graph/mtl/FixedMaterialDef.h>
#include <hgl/shadergen/ShaderComposition.h>
#include <hgl/vk/VKRenderAssign.h>
#include <cstdio>
#include <vector>

#include "S_BasicLit_Logic.h"

namespace hgl::graph::mtl{
namespace
{
    constexpr const char mi_codes[] = R"(
        uint base_color;
        float metallic;
        float roughness;
        float fresnel;
        float ibl_intensity;
        float normal_strength;
    )";
    constexpr const uint32_t mi_bytes = sizeof(uint32_t) + sizeof(float) * 5;

    constexpr FixedVertexEntry BASIC_LIT_VERTEX[] = {
        { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Position },
        { VAT_VEC2, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::TexCoord },
        { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Normal },
        { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VertexInputRate::Instance, Assign::TransformID::VIS_NAME },
        { Assign::MaterialInstanceID::VAT_FMT, VertexInputGroup::MaterialInstanceID, VertexInputRate::Instance, Assign::MaterialInstanceID::VIS_NAME },
    };

#if defined(HGL_L2W_USE_SSBO) && HGL_L2W_USE_SSBO
    constexpr DescriptorKind BASIC_LIT_L2W_KIND = DescriptorKind::SSBO;
#else
    constexpr DescriptorKind BASIC_LIT_L2W_KIND = DescriptorKind::UBO;
#endif

    constexpr FixedDescriptorEntry BASIC_LIT_DESCRIPTORS[] = {
        { DescriptorSetType::RenderTarget, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
        { DescriptorSetType::Camera, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
        { DescriptorSetType::Camera, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "sky", "SkyInfo", nullptr },
        { DescriptorSetType::PerFrame, BASIC_LIT_L2W_KIND, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
        { DescriptorSetType::PerMaterial, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr },
        { DescriptorSetType::PerMaterial, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2D" },
        { DescriptorSetType::PerMaterial, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureNormal", nullptr, "sampler2D" },
        { DescriptorSetType::PerMaterial, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureRoughness", nullptr, "sampler2D" },
    };

    constexpr VertexShaderBusiness BASIC_LIT_VERTEX_BUSINESS { BASIC_LIT_VS_BUSINESS };
    constexpr FragmentShaderBusiness BASIC_LIT_FRAGMENT_BUSINESS { BASIC_LIT_FS_BUSINESS };

    constexpr FixedMaterialDef BASIC_LIT_DEF {
        "BasicLit_v2",
        PrimitiveType::Triangles,
        BASIC_LIT_VERTEX,
        uint32_t(sizeof(BASIC_LIT_VERTEX) / sizeof(BASIC_LIT_VERTEX[0])),
        BASIC_LIT_DESCRIPTORS,
        uint32_t(sizeof(BASIC_LIT_DESCRIPTORS) / sizeof(BASIC_LIT_DESCRIPTORS[0])),
        mi_codes,
        mi_bytes,
    };

    const ComposedMaterialDef BASIC_LIT_COMPOSED_DEF {
        "BasicLit_v2",
        PrimitiveType::Triangles,
        BASIC_LIT_VERTEX,
        uint32_t(sizeof(BASIC_LIT_VERTEX) / sizeof(BASIC_LIT_VERTEX[0])),
        BASIC_LIT_DESCRIPTORS,
        uint32_t(sizeof(BASIC_LIT_DESCRIPTORS) / sizeof(BASIC_LIT_DESCRIPTORS[0])),
        &BASIC_LIT_VERTEX_BUSINESS,
        &BASIC_LIT_FRAGMENT_BUSINESS,
        ShaderOutputMode::SingleRTAlphaBlend,
        false,
        mi_codes,
        mi_bytes,
    };

}

MaterialCreateInfo *CreateBasicLit(const contract::PhysicalDeviceProfileLite *profile, BasicLitMaterialCreateConfig *cfg)
{
    if(cfg)
        cfg->material_instance=true;

    ShaderPermutationKey key;
    key.ambient = cfg ? cfg->sky_ambient_model : SkyLightAmbientModel::Simple;

    std::vector<FixedDescriptorEntry> dynamic_descriptors(
        BASIC_LIT_DESCRIPTORS,
        BASIC_LIT_DESCRIPTORS + uint32_t(sizeof(BASIC_LIT_DESCRIPTORS) / sizeof(BASIC_LIT_DESCRIPTORS[0])));

    std::vector<const char *> dynamic_fragment_resources(
        BASIC_LIT_FRAGMENT_RESOURCES,
        BASIC_LIT_FRAGMENT_RESOURCES + uint32_t(sizeof(BASIC_LIT_FRAGMENT_RESOURCES) / sizeof(BASIC_LIT_FRAGMENT_RESOURCES[0])));

    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(key.ambient),
        dynamic_descriptors,
        dynamic_fragment_resources);

    MaterialLogicDef dynamic_logic = BASIC_LIT_LOGIC;
    dynamic_logic.fragment.required_resources = dynamic_fragment_resources.data();
    dynamic_logic.fragment.required_resource_count = uint32_t(dynamic_fragment_resources.size());

    FixedMaterialDef dynamic_fixed_def = BASIC_LIT_DEF;
    dynamic_fixed_def.descriptor_entries = dynamic_descriptors.data();
    dynamic_fixed_def.descriptor_entry_count = uint32_t(dynamic_descriptors.size());

    ComposedMaterialDef dynamic_composed_def = BASIC_LIT_COMPOSED_DEF;
    dynamic_composed_def.descriptor_entries = dynamic_descriptors.data();
    dynamic_composed_def.descriptor_entry_count = uint32_t(dynamic_descriptors.size());

    MaterialCreateInfo *mci_new = CompileComposedBusinessMaterial(
        profile,
        dynamic_fixed_def,
        dynamic_composed_def,
        dynamic_logic,
        key,
        cfg);

    if (!mci_new)
        std::fprintf(stderr, "[BasicLit] CompileComposedBusinessMaterial failed\n");
    return mci_new;
}
}//namespace hgl::graph::mtl


