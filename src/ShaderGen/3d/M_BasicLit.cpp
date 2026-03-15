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

    constexpr FixedDescriptorEntry BASIC_LIT_DESCRIPTORS[] = {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "sky", "SkyInfo", nullptr },
        { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
        { DescriptorSetType::Material, MaterialInstanceDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr },
        { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2D" },
        { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureNormal", nullptr, "sampler2D" },
        { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureRoughness", nullptr, "sampler2D" },
    };

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

}

MaterialCreateInfo *CreateBasicLit(const contract::PhysicalDeviceProfileLite *profile, BasicLitMaterialCreateConfig *cfg)
{
    if(cfg)
        cfg->material_instance=true;

    // Dynamic descriptor injection for non-Simple sky models
    SkyLightAmbientModel ambient = cfg ? cfg->sky_ambient_model : SkyLightAmbientModel::Simple;

    std::vector<FixedDescriptorEntry> dynamic_descriptors(
        BASIC_LIT_DESCRIPTORS,
        BASIC_LIT_DESCRIPTORS + uint32_t(sizeof(BASIC_LIT_DESCRIPTORS) / sizeof(BASIC_LIT_DESCRIPTORS[0])));

    std::vector<const char *> unused_resources;
    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(ambient),
        dynamic_descriptors,
        unused_resources);

    FixedMaterialDef dynamic_def = BASIC_LIT_DEF;
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
        "surface/basiclit_surface.glsl"
    );

    if (!result.success)
    {
        std::fprintf(stderr, "[BasicLit] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[BasicLit] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl


