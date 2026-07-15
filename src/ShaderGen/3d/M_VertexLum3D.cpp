#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/common/RenderAssignDef.h>
#include<cstdio>
#include<string>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char VERTEX_LUMINANCE_3D_MI_CODES[] = "vec4 Color;";
    constexpr const uint32_t VERTEX_LUMINANCE_3D_MI_BYTES = sizeof(hgl::math::Vector4f);

    constexpr FixedVertexEntry VERTEX_LUMINANCE_3D_VERTEX_VEC3[] = {
        { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Position },
        { VAT_FLOAT, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Luminance },
    };

    constexpr FixedVertexEntry VERTEX_LUMINANCE_3D_VERTEX_VEC2[] = {
        { VAT_VEC2, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Position },
        { VAT_FLOAT, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Luminance },
    };

      constexpr FixedDescriptorEntry VERTEX_LUMINANCE_3D_DESCRIPTORS[] = {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
        { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr },
        { DescriptorSetType::Material, MaterialInstanceDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr },
        { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows", "DataIndexRows", nullptr },
        { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_texture_layer_rows", "TextureLayerRows", nullptr },
    };

    constexpr FixedMaterialDef VERTEX_LUMINANCE_3D_DEF_VEC3 {
        "VertexLuminance3D",
        PrimitiveType::Triangles,
        VERTEX_LUMINANCE_3D_VERTEX_VEC3,
        uint32_t(sizeof(VERTEX_LUMINANCE_3D_VERTEX_VEC3) / sizeof(VERTEX_LUMINANCE_3D_VERTEX_VEC3[0])),
        VERTEX_LUMINANCE_3D_DESCRIPTORS,
        uint32_t(sizeof(VERTEX_LUMINANCE_3D_DESCRIPTORS) / sizeof(VERTEX_LUMINANCE_3D_DESCRIPTORS[0])),
        VERTEX_LUMINANCE_3D_MI_CODES,
        VERTEX_LUMINANCE_3D_MI_BYTES,
    };

    constexpr FixedMaterialDef VERTEX_LUMINANCE_3D_DEF_VEC2 {
        "VertexLuminance3D",
        PrimitiveType::Triangles,
        VERTEX_LUMINANCE_3D_VERTEX_VEC2,
        uint32_t(sizeof(VERTEX_LUMINANCE_3D_VERTEX_VEC2) / sizeof(VERTEX_LUMINANCE_3D_VERTEX_VEC2[0])),
        VERTEX_LUMINANCE_3D_DESCRIPTORS,
        uint32_t(sizeof(VERTEX_LUMINANCE_3D_DESCRIPTORS) / sizeof(VERTEX_LUMINANCE_3D_DESCRIPTORS[0])),
        VERTEX_LUMINANCE_3D_MI_CODES,
        VERTEX_LUMINANCE_3D_MI_BYTES,
    };
}

MaterialCreateInfo *CreateVertexLuminance3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    cfg->material_instance=true;

    const bool use_vec2_position = cfg && cfg->position_format.ToCode() == VAT_VEC2.ToCode();

    const FixedMaterialDef &fixed_def = use_vec2_position
        ? VERTEX_LUMINANCE_3D_DEF_VEC2
        : VERTEX_LUMINANCE_3D_DEF_VEC3;

    // 通过 CompositorAssembler 从 .glsl 模板文件组装 VS/FS
    CompositorAssembler assembler("ShaderLibrary");

    const char *vs_template = use_vec2_position
        ? "compositor/main_forward_unlit_luminance_2d.vert.glsl"
        : "compositor/main_forward_unlit_luminance.vert.glsl";

    auto result = assembler.Assemble(
        SurfaceType::Unlit,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        vs_template,                                            // VS: Pos+Lum+TID+MIID
        "compositor/main_forward_unlit_luminance.frag.glsl",    // FS: luminance+MIID
        "surface/unlit_luminance_surface.glsl"                  // Surface: MI color × luminance
    );

    if (!result.success)
    {
        std::fprintf(stderr, "[VertexLuminance3D] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        fixed_def,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[VertexLuminance3D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
