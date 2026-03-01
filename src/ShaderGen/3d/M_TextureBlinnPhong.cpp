#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/graph/mtl/UBOCommon.h>
#include <hgl/graph/mtl/MaterialCompiler.h>
#include <hgl/graph/mtl/Material3DCreateConfig.h>
#include <hgl/graph/mtl/FixedMaterialDef.h>
#include <hgl/graph/mtl/ShaderComposition.h>
#include <hgl/vk/VKRenderAssign.h>
#include <cstdio>

#include "S_TextureBlinnPhong_Logic.h"

namespace hgl::graph::mtl{
namespace
{
    constexpr const char mi_codes[] = R"(
        float normal_strength;
    )";
    constexpr const uint32_t mi_bytes = sizeof(float);

    constexpr FixedVertexEntry TEXTURE_BLINN_PHONG_VERTEX[] = {
        { VAT_VEC3, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::Position },
        { VAT_VEC2, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::TexCoord },
        { VAT_VEC3, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::Normal },
        { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VK_VERTEX_INPUT_RATE_INSTANCE, Assign::TransformID::VIS_NAME },
        { Assign::MaterialInstanceID::VAT_FMT, VertexInputGroup::MaterialInstanceID, VK_VERTEX_INPUT_RATE_INSTANCE, Assign::MaterialInstanceID::VIS_NAME },
    };

#if defined(HGL_L2W_USE_SSBO) && HGL_L2W_USE_SSBO
    constexpr DescriptorKind TEXTURE_BLINN_PHONG_L2W_KIND = DescriptorKind::SSBO;
#else
    constexpr DescriptorKind TEXTURE_BLINN_PHONG_L2W_KIND = DescriptorKind::UBO;
#endif

    constexpr FixedDescriptorEntry TEXTURE_BLINN_PHONG_DESCRIPTORS[] = {
        { DescriptorSetType::RenderTarget, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
        { DescriptorSetType::Camera, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
        { DescriptorSetType::Camera, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "sky", "SkyInfo", nullptr },
        { DescriptorSetType::PerFrame, TEXTURE_BLINN_PHONG_L2W_KIND, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
        { DescriptorSetType::PerMaterial, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr },
        { DescriptorSetType::PerMaterial, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2D" },
        { DescriptorSetType::PerMaterial, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureNormal", nullptr, "sampler2D" },
        { DescriptorSetType::PerMaterial, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureRoughness", nullptr, "sampler2D" },
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
MaterialCreateInfo *CreateTextureBlinnPhong(const VulkanDevAttr *dev_attr, const Material3DCreateConfig *cfg)
{
    Material3DCreateConfig cfg_with_mi = cfg ? *cfg : Material3DCreateConfig();
    cfg_with_mi.material_instance = true;

    ShaderPermutationKey key;
    key.ambient = cfg_with_mi.sky_ambient_model;
    MaterialCreateInfo *mci_new = CompileComposedBusinessMaterial(
        dev_attr,
        TEXTURE_BLINN_PHONG_DEF,
        TEXTURE_BLINN_PHONG_COMPOSED_DEF,
        TEXTURE_BLINN_PHONG_LOGIC,
        key,
        &cfg_with_mi);

    if (!mci_new)
        std::fprintf(stderr, "[TextureBlinnPhong] CompileComposedBusinessMaterial failed\n");
    return mci_new;
}
}//namespace hgl::graph::mtl
