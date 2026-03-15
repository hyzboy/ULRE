#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/common/RenderAssignDef.h>
#include <cstdio>
#include <vector>

#include "../common/MFSkyLight.h"

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

    constexpr FixedDescriptorEntry TEXTURE_BLINN_PHONG_DESCRIPTORS[] = {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "sky", "SkyInfo", nullptr },
        { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
        { DescriptorSetType::Material, MaterialInstanceDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr },
        { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2D" },
        { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureNormal", nullptr, "sampler2D" },
        { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureRoughness", nullptr, "sampler2D" },
    };

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

}

MaterialCreateInfo *CreateTextureBlinnPhong(const contract::PhysicalDeviceProfileLite *profile, const Material3DCreateConfig *cfg)
{
    Material3DCreateConfig cfg_with_mi = cfg ? *cfg : Material3DCreateConfig();
    cfg_with_mi.material_instance = true;

    // Dynamic descriptor injection for non-Simple sky models
    SkyLightAmbientModel ambient = cfg_with_mi.sky_ambient_model;

    std::vector<FixedDescriptorEntry> dynamic_descriptors(
        TEXTURE_BLINN_PHONG_DESCRIPTORS,
        TEXTURE_BLINN_PHONG_DESCRIPTORS + uint32_t(sizeof(TEXTURE_BLINN_PHONG_DESCRIPTORS) / sizeof(TEXTURE_BLINN_PHONG_DESCRIPTORS[0])));

    std::vector<const char *> unused_resources;
    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(ambient),
        dynamic_descriptors,
        unused_resources);

    FixedMaterialDef dynamic_def = TEXTURE_BLINN_PHONG_DEF;
    dynamic_def.descriptor_entries = dynamic_descriptors.data();
    dynamic_def.descriptor_entry_count = uint32_t(dynamic_descriptors.size());

    // Assemble GLSL from templates
    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(
        SurfaceType::Standard,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        "compositor/main_forward_lit.vert.glsl",
        "compositor/main_forward_lit.frag.glsl",
        "surface/textureblinnphong_surface.glsl"
    );

    if (!result.success)
    {
        std::fprintf(stderr, "[TextureBlinnPhong] CompositorAssembler failed: %s\n",
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
        std::fprintf(stderr, "[TextureBlinnPhong] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl


