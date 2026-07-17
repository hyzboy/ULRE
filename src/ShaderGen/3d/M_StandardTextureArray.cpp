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
        uint  base_color;
        float metallic;
        float roughness;
        float normal_scale;
        uint  texture_id;
    )";
    constexpr const uint32_t mi_bytes = sizeof(uint32_t) * 2 + sizeof(float) * 3;

    constexpr FixedVertexEntry STANDARD_ARRAY_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_VEC2, VAN::TexCoord },
        { VAT_VEC3, VAN::Normal },
    };

    constexpr FixedDescriptorEntry STANDARD_ARRAY_DESCRIPTORS[] = {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "sky", "SkyInfo", nullptr },
        { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr },
        { DescriptorSetType::Material, MaterialInstanceDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr },
        { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows", "DataIndexRows", nullptr },
        { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_texture_layer_rows", "TextureLayerRows", nullptr },
        { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2DArray" },
        { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureNormal", nullptr, "sampler2DArray" },
        { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureRoughness", nullptr, "sampler2DArray" },
    };

    constexpr FixedMaterialDef STANDARD_ARRAY_DEF {
        "StandardTextureArray_v1",
        PrimitiveType::Triangles,
        STANDARD_ARRAY_VERTEX,
        uint32_t(sizeof(STANDARD_ARRAY_VERTEX) / sizeof(STANDARD_ARRAY_VERTEX[0])),
        STANDARD_ARRAY_DESCRIPTORS,
        uint32_t(sizeof(STANDARD_ARRAY_DESCRIPTORS) / sizeof(STANDARD_ARRAY_DESCRIPTORS[0])),
        mi_codes,
        mi_bytes,
    };

}

MaterialCreateInfo *CreateStandardTextureArray(const contract::PhysicalDeviceProfileLite *profile, const Material3DCreateConfig *cfg)
{
    Material3DCreateConfig cfg_with_mi = cfg ? *cfg : Material3DCreateConfig();
    cfg_with_mi.material_instance = true;

    SkyLightAmbientModel ambient = cfg_with_mi.sky_ambient_model;

    std::vector<FixedDescriptorEntry> dynamic_descriptors(
        STANDARD_ARRAY_DESCRIPTORS,
        STANDARD_ARRAY_DESCRIPTORS + uint32_t(sizeof(STANDARD_ARRAY_DESCRIPTORS) / sizeof(STANDARD_ARRAY_DESCRIPTORS[0])));

    std::vector<const char *> unused_resources;
    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(ambient),
        dynamic_descriptors,
        unused_resources);

    FixedMaterialDef dynamic_def = STANDARD_ARRAY_DEF;
    dynamic_def.descriptor_entries = dynamic_descriptors.data();
    dynamic_def.descriptor_entry_count = uint32_t(dynamic_descriptors.size());

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(
        SurfaceType::Standard,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        "compositor/main_forward_lit.vert.glsl",
        "compositor/main_forward_lit.frag.glsl",
        "surface/standard_texturearray_surface.glsl"
    );

    if (!result.success)
    {
        std::fprintf(stderr, "[StandardTextureArray] CompositorAssembler failed: %s\n",
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
        std::fprintf(stderr, "[StandardTextureArray] CompileCompositorMaterial failed\n");
    return mci;
}

}//namespace hgl::graph::mtl

