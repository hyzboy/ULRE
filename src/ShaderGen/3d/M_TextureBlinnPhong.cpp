#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/FixedMaterialDef.h>
#include <hgl/shadergen/ShaderComposition.h>
#include <hgl/common/RenderAssignDef.h>
#include <cstdio>
#include <vector>

#include "S_TextureBlinnPhong_Logic.h"

namespace hgl::graph::mtl{
namespace
{
    constexpr const char mi_codes[] = R"(
        float normal_strength;
    )";
    constexpr const uint32_t mi_bytes = sizeof(float);

    constexpr FixedVertexEntry TEXTURE_BLINN_PHONG_VERTEX[] = {
        { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Position },
        { VAT_VEC2, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::TexCoord },
        { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Normal },
        { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VertexInputRate::Instance, Assign::TransformID::VIS_NAME },
        { Assign::MaterialInstanceID::VAT_FMT, VertexInputGroup::MaterialInstanceID, VertexInputRate::Instance, Assign::MaterialInstanceID::VIS_NAME },
    };

#if defined(HGL_L2W_USE_SSBO) && HGL_L2W_USE_SSBO
    constexpr DescriptorKind TEXTURE_BLINN_PHONG_L2W_KIND = DescriptorKind::SSBO;
#else
    constexpr DescriptorKind TEXTURE_BLINN_PHONG_L2W_KIND = DescriptorKind::UBO;
#endif

    constexpr FixedDescriptorEntry TEXTURE_BLINN_PHONG_DESCRIPTORS[] = {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "sky", "SkyInfo", nullptr },
        { DescriptorSetType::Transform, TEXTURE_BLINN_PHONG_L2W_KIND, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
        { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr },
        { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2D" },
        { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureNormal", nullptr, "sampler2D" },
        { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureRoughness", nullptr, "sampler2D" },
    };

    constexpr VertexShaderBusiness TEXTURE_BLINN_PHONG_VERTEX_BUSINESS { TEXTURE_BLINN_PHONG_VS_BUSINESS };
    constexpr FragmentShaderBusiness TEXTURE_BLINN_PHONG_FRAGMENT_BUSINESS { TEXTURE_BLINN_PHONG_FS_BUSINESS };

    constexpr FixedMaterialDef TEXTURE_BLINN_PHONG_DEF {
        "TextureBlinnPhong_v2",
        PrimitiveType::Triangles,
        TEXTURE_BLINN_PHONG_VERTEX,
        uint32_t(sizeof(TEXTURE_BLINN_PHONG_VERTEX) / sizeof(TEXTURE_BLINN_PHONG_VERTEX[0])),
        TEXTURE_BLINN_PHONG_DESCRIPTORS,
        uint32_t(sizeof(TEXTURE_BLINN_PHONG_DESCRIPTORS) / sizeof(TEXTURE_BLINN_PHONG_DESCRIPTORS[0])),
        mi_codes,
        mi_bytes,
    };

    const ComposedMaterialDef TEXTURE_BLINN_PHONG_COMPOSED_DEF {
        "TextureBlinnPhong_v2",
        PrimitiveType::Triangles,
        TEXTURE_BLINN_PHONG_VERTEX,
        uint32_t(sizeof(TEXTURE_BLINN_PHONG_VERTEX) / sizeof(TEXTURE_BLINN_PHONG_VERTEX[0])),
        TEXTURE_BLINN_PHONG_DESCRIPTORS,
        uint32_t(sizeof(TEXTURE_BLINN_PHONG_DESCRIPTORS) / sizeof(TEXTURE_BLINN_PHONG_DESCRIPTORS[0])),
        &TEXTURE_BLINN_PHONG_VERTEX_BUSINESS,
        &TEXTURE_BLINN_PHONG_FRAGMENT_BUSINESS,
        ShaderOutputMode::SingleRTAlphaBlend,
        false,
        mi_codes,
        mi_bytes,
    };

}

// Factory
MaterialCreateInfo *CreateTextureBlinnPhong(const contract::PhysicalDeviceProfileLite *profile, const Material3DCreateConfig *cfg)
{
    Material3DCreateConfig cfg_with_mi = cfg ? *cfg : Material3DCreateConfig();
    cfg_with_mi.material_instance = true;

    ShaderPermutationKey key;
    key.ambient = cfg_with_mi.sky_ambient_model;

    std::vector<FixedDescriptorEntry> dynamic_descriptors(
        TEXTURE_BLINN_PHONG_DESCRIPTORS,
        TEXTURE_BLINN_PHONG_DESCRIPTORS + uint32_t(sizeof(TEXTURE_BLINN_PHONG_DESCRIPTORS) / sizeof(TEXTURE_BLINN_PHONG_DESCRIPTORS[0])));

    std::vector<const char *> dynamic_fragment_resources(
        TEXTURE_BLINN_PHONG_FRAGMENT_RESOURCES,
        TEXTURE_BLINN_PHONG_FRAGMENT_RESOURCES + uint32_t(sizeof(TEXTURE_BLINN_PHONG_FRAGMENT_RESOURCES) / sizeof(TEXTURE_BLINN_PHONG_FRAGMENT_RESOURCES[0])));

    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(key.ambient),
        dynamic_descriptors,
        dynamic_fragment_resources);

    MaterialLogicDef dynamic_logic = TEXTURE_BLINN_PHONG_LOGIC;
    dynamic_logic.fragment.required_resources = dynamic_fragment_resources.data();
    dynamic_logic.fragment.required_resource_count = uint32_t(dynamic_fragment_resources.size());

    FixedMaterialDef dynamic_fixed_def = TEXTURE_BLINN_PHONG_DEF;
    dynamic_fixed_def.descriptor_entries = dynamic_descriptors.data();
    dynamic_fixed_def.descriptor_entry_count = uint32_t(dynamic_descriptors.size());

    ComposedMaterialDef dynamic_composed_def = TEXTURE_BLINN_PHONG_COMPOSED_DEF;
    dynamic_composed_def.descriptor_entries = dynamic_descriptors.data();
    dynamic_composed_def.descriptor_entry_count = uint32_t(dynamic_descriptors.size());

    MaterialCreateInfo *mci_new = CompileComposedBusinessMaterial(
        profile,
        dynamic_fixed_def,
        dynamic_composed_def,
        dynamic_logic,
        key,
        &cfg_with_mi);

    if (!mci_new)
        std::fprintf(stderr, "[TextureBlinnPhong] CompileComposedBusinessMaterial failed\n");
    return mci_new;
}
}//namespace hgl::graph::mtl


