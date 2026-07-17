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
    constexpr const char pure_color_3d_mi_codes[] = "vec4 Color;";
    constexpr const uint32_t pure_color_3d_mi_bytes = 16;

    constexpr FixedVertexEntry PURE_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
    };

    constexpr FixedDescriptorEntry PURE_COLOR_3D_DESCRIPTORS[] = {
        { DescriptorSetType::Scene,     DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
        { DescriptorSetType::Scene,     DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera",   "CameraInfo",   nullptr },
        { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr },
        { DescriptorSetType::Material,  MaterialInstanceDescriptorKind,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr },
        { DescriptorSetType::Material,  DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows", "DataIndexRows", nullptr },
        { DescriptorSetType::Material,  DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_texture_layer_rows", "TextureLayerRows", nullptr },
    };

    constexpr FixedMaterialDef PURE_COLOR_3D_DEF {
        "PureColor3D",
        PrimitiveType::Triangles,
        PURE_COLOR_3D_VERTEX,
        uint32_t(sizeof(PURE_COLOR_3D_VERTEX) / sizeof(PURE_COLOR_3D_VERTEX[0])),
        PURE_COLOR_3D_DESCRIPTORS,
        uint32_t(sizeof(PURE_COLOR_3D_DESCRIPTORS) / sizeof(PURE_COLOR_3D_DESCRIPTORS[0])),
        pure_color_3d_mi_codes,
        pure_color_3d_mi_bytes,
    };
}

MaterialCreateInfo *CreatePureColor3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    // 通过 CompositorAssembler 从 .glsl 模板文件组装 VS/FS
    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(
        SurfaceType::Unlit,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC
    );

    if (!result.success)
    {
        std::fprintf(stderr, "[PureColor3D] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        PURE_COLOR_3D_DEF,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[PureColor3D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl

