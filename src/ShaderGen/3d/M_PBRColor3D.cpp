#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/FixedMaterialDef.h>
#include <hgl/shadergen/ShaderComposition.h>
#include <hgl/common/RenderAssignDef.h>
#include <cstdio>
#include <vector>

#include "S_PBRColor3D_Logic.h"

namespace hgl::graph::mtl{
namespace
{
    // MI layout (matches PBRColor3DMaterialInstance in Material3DCreateConfig.h)
    //   uint  base_color  — RGBA packed, recovered via unpackUnorm4x8
    //   float metallic    — [0, 1]
    //   float roughness   — [0.04, 1]
    //   uint  texture_id  — Texture2DArray layer index
    constexpr const char mi_codes[] = R"(
        uint  base_color;
        float metallic;
        float roughness;
        uint  texture_id;
    )";
    constexpr const uint32_t mi_bytes = sizeof(uint32_t) * 2 + sizeof(float) * 2;

    constexpr FixedVertexEntry PBR_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex,   VAN::Position },
        { VAT_VEC2, VertexInputGroup::Basic, VertexInputRate::Vertex,   VAN::TexCoord },
        { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex,   VAN::Normal   },
        { Assign::TransformID::VAT_FMT,        VertexInputGroup::TransformID,        VertexInputRate::Instance, Assign::TransformID::VIS_NAME        },
        { Assign::MaterialInstanceID::VAT_FMT, VertexInputGroup::MaterialInstanceID, VertexInputRate::Instance, Assign::MaterialInstanceID::VIS_NAME },
    };

#if defined(HGL_L2W_USE_SSBO) && HGL_L2W_USE_SSBO
    constexpr DescriptorKind PBR_COLOR_3D_L2W_KIND = DescriptorKind::SSBO;
#else
    constexpr DescriptorKind PBR_COLOR_3D_L2W_KIND = DescriptorKind::UBO;
#endif

    constexpr FixedDescriptorEntry PBR_COLOR_3D_DESCRIPTORS[] = {
        { DescriptorSetType::Scene,      DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo",        nullptr },
        { DescriptorSetType::Scene,      DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera",   "CameraInfo",          nullptr },
        { DescriptorSetType::Scene,      DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "sky",      "SkyInfo",             nullptr },
        { DescriptorSetType::Transform,  PBR_COLOR_3D_L2W_KIND,uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w",      "LocalToWorldData",    nullptr },
        { DescriptorSetType::Material,   DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl",      "MaterialInstanceData",nullptr },
        { DescriptorSetType::Material,   DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2DArray" },
        { DescriptorSetType::Material,   DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureNormal",    nullptr, "sampler2DArray" },
    };

    constexpr VertexShaderBusiness   PBR_COLOR_3D_VERTEX_BUSINESS   { PBR_COLOR_3D_VS_BUSINESS };
    constexpr FragmentShaderBusiness  PBR_COLOR_3D_FRAGMENT_BUSINESS { PBR_COLOR_3D_FS_BUSINESS };

    constexpr FixedMaterialDef PBR_COLOR_3D_DEF {
        "PBRColor3D",
        PrimitiveType::Triangles,
        PBR_COLOR_3D_VERTEX,
        uint32_t(sizeof(PBR_COLOR_3D_VERTEX)      / sizeof(PBR_COLOR_3D_VERTEX[0])),
        PBR_COLOR_3D_DESCRIPTORS,
        uint32_t(sizeof(PBR_COLOR_3D_DESCRIPTORS) / sizeof(PBR_COLOR_3D_DESCRIPTORS[0])),
        mi_codes,
        mi_bytes,
    };

    const ComposedMaterialDef PBR_COLOR_3D_COMPOSED_DEF {
        "PBRColor3D",
        PrimitiveType::Triangles,
        PBR_COLOR_3D_VERTEX,
        uint32_t(sizeof(PBR_COLOR_3D_VERTEX)      / sizeof(PBR_COLOR_3D_VERTEX[0])),
        PBR_COLOR_3D_DESCRIPTORS,
        uint32_t(sizeof(PBR_COLOR_3D_DESCRIPTORS) / sizeof(PBR_COLOR_3D_DESCRIPTORS[0])),
        &PBR_COLOR_3D_VERTEX_BUSINESS,
        &PBR_COLOR_3D_FRAGMENT_BUSINESS,
        ShaderOutputMode::SingleRTAlphaBlend,
        false,
        mi_codes,
        mi_bytes,
    };

}//namespace

MaterialCreateInfo *CreatePBRColor3D(const contract::PhysicalDeviceProfileLite *profile, PBRColor3DMaterialCreateConfig *cfg)
{
    if (cfg)
        cfg->material_instance = true;

    ShaderPermutationKey key;
    key.ambient = cfg ? cfg->sky_ambient_model : SkyLightAmbientModel::Simple;

    std::vector<FixedDescriptorEntry> dynamic_descriptors(
        PBR_COLOR_3D_DESCRIPTORS,
        PBR_COLOR_3D_DESCRIPTORS + uint32_t(sizeof(PBR_COLOR_3D_DESCRIPTORS) / sizeof(PBR_COLOR_3D_DESCRIPTORS[0])));

    std::vector<const char *> dynamic_fragment_resources(
        PBR_COLOR_3D_FRAGMENT_RESOURCES,
        PBR_COLOR_3D_FRAGMENT_RESOURCES + uint32_t(sizeof(PBR_COLOR_3D_FRAGMENT_RESOURCES) / sizeof(PBR_COLOR_3D_FRAGMENT_RESOURCES[0])));

    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(key.ambient),
        dynamic_descriptors,
        dynamic_fragment_resources);

    MaterialLogicDef dynamic_logic = PBR_COLOR_3D_LOGIC;
    dynamic_logic.fragment.required_resources       = dynamic_fragment_resources.data();
    dynamic_logic.fragment.required_resource_count  = uint32_t(dynamic_fragment_resources.size());

    FixedMaterialDef dynamic_fixed_def = PBR_COLOR_3D_DEF;
    dynamic_fixed_def.descriptor_entries      = dynamic_descriptors.data();
    dynamic_fixed_def.descriptor_entry_count  = uint32_t(dynamic_descriptors.size());

    ComposedMaterialDef dynamic_composed_def = PBR_COLOR_3D_COMPOSED_DEF;
    dynamic_composed_def.descriptor_entries     = dynamic_descriptors.data();
    dynamic_composed_def.descriptor_entry_count = uint32_t(dynamic_descriptors.size());

    MaterialCreateInfo *mci = CompileComposedBusinessMaterial(
        profile,
        dynamic_fixed_def,
        dynamic_composed_def,
        dynamic_logic,
        key,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[PBRColor3D] CompileComposedBusinessMaterial failed\n");

    return mci;
}

}//namespace hgl::graph::mtl
