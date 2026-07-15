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
    )";
    constexpr const uint32_t mi_bytes = sizeof(uint32_t) + sizeof(float) * 2;

    constexpr FixedVertexEntry PBR_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex,   VAN::Position },
        { VAT_VEC2, VertexInputGroup::Basic, VertexInputRate::Vertex,   VAN::TexCoord },
        { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex,   VAN::Normal   },
        { Assign::TransformID::VAT_FMT,        VertexInputGroup::TransformID,        VertexInputRate::Instance, Assign::TransformID::VIS_NAME        },
        { Assign::DataIndexID::VAT_FMT,    VertexInputGroup::DataIndexID,    VertexInputRate::Instance, Assign::DataIndexID::VIS_NAME },
        { Assign::TextureLayerID::VAT_FMT, VertexInputGroup::TextureLayerID, VertexInputRate::Instance, Assign::TextureLayerID::VIS_NAME },
    };

    constexpr FixedDescriptorEntry PBR_COLOR_3D_DESCRIPTORS[] = {
        { DescriptorSetType::Scene,      DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo",        nullptr },
        { DescriptorSetType::Scene,      DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera",   "CameraInfo",          nullptr },
        { DescriptorSetType::Scene,      DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "sky",      "SkyInfo",             nullptr },
        { DescriptorSetType::Transform,  TransformDescriptorKind,uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w",      "LocalToWorldData",    nullptr },
        { DescriptorSetType::Transform,  DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr },
        { DescriptorSetType::Material,   MaterialInstanceDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl",      "MaterialInstanceData", nullptr },
        { DescriptorSetType::Material,   DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows", "DataIndexRows", nullptr },
        { DescriptorSetType::Material,   DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_texture_layer_rows", "TextureLayerRows", nullptr },
    };

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

}//namespace

MaterialCreateInfo *CreatePBRColor3D(const contract::PhysicalDeviceProfileLite *profile, PBRColor3DMaterialCreateConfig *cfg)
{
    if (cfg)
        cfg->material_instance = true;

    // Dynamic descriptor injection for non-Simple sky models
    SkyLightAmbientModel ambient = cfg ? cfg->sky_ambient_model : SkyLightAmbientModel::Simple;

    std::vector<FixedDescriptorEntry> dynamic_descriptors(
        PBR_COLOR_3D_DESCRIPTORS,
        PBR_COLOR_3D_DESCRIPTORS + uint32_t(sizeof(PBR_COLOR_3D_DESCRIPTORS) / sizeof(PBR_COLOR_3D_DESCRIPTORS[0])));

    std::vector<const char *> unused_resources;
    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(ambient),
        dynamic_descriptors,
        unused_resources);

    FixedMaterialDef dynamic_def = PBR_COLOR_3D_DEF;
    dynamic_def.descriptor_entries      = dynamic_descriptors.data();
    dynamic_def.descriptor_entry_count  = uint32_t(dynamic_descriptors.size());

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
        "surface/pbrcolor3d_surface.glsl"
    );

    if (!result.success)
    {
        std::fprintf(stderr, "[PBRColor3D] CompositorAssembler failed: %s\n",
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
        std::fprintf(stderr, "[PBRColor3D] CompileCompositorMaterial failed\n");

    return mci;
}

}//namespace hgl::graph::mtl
