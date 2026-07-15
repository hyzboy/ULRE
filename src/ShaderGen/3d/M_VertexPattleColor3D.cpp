/** 顶点调色板色要求有一个UBO结构如下
*
*
*   struct ColorPattle
*   {
*       vec4 color[256];
*   }color_pattle;
*
*   然后输入的一个R8UI顶点属性来指定使用那个颜色。
*/

#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/common/RenderAssignDef.h>
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry VERTEX_PATTLE_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Position },
        { VAT_UINT, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Color },
        { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VertexInputRate::Vertex, Assign::TransformID::VIS_NAME },
    };

    constexpr FixedDescriptorEntry VERTEX_PATTLE_COLOR_3D_DESCRIPTORS[] = {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
        { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr },
        { DescriptorSetType::Material, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "color_pattle", "ColorPattle", nullptr },
    };

    constexpr FixedMaterialDef VERTEX_PATTLE_COLOR_3D_DEF {
        "VertexPattleColor3D",
        PrimitiveType::Triangles,
        VERTEX_PATTLE_COLOR_3D_VERTEX,
        uint32_t(sizeof(VERTEX_PATTLE_COLOR_3D_VERTEX) / sizeof(VERTEX_PATTLE_COLOR_3D_VERTEX[0])),
        VERTEX_PATTLE_COLOR_3D_DESCRIPTORS,
        uint32_t(sizeof(VERTEX_PATTLE_COLOR_3D_DESCRIPTORS) / sizeof(VERTEX_PATTLE_COLOR_3D_DESCRIPTORS[0])),
        nullptr,
        0,
    };
}//namespace

MaterialCreateInfo *CreateVertexPattleColor3D(const contract::PhysicalDeviceProfileLite *profile,const Material3DCreateConfig *cfg)
{
    Material3DCreateConfig local_cfg = cfg ? *cfg : Material3DCreateConfig();

    static const ShaderBufferSource * const private_sbs_list[] =
    {
        &SBS_ColorPattle
    };
    local_cfg.SetPrivateShaderBufferSources(private_sbs_list,1);

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(
        SurfaceType::Unlit,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        "compositor/main_forward_unlit_pattle.vert.glsl",
        "compositor/main_forward_unlit_vertexcolor.frag.glsl",
        "surface/unlit_vertexcolor_surface.glsl"
    );

    if (!result.success)
    {
        std::fprintf(stderr, "[VertexPattleColor3D] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        VERTEX_PATTLE_COLOR_3D_DEF,
        result.vertex_glsl,
        result.fragment_glsl,
        &local_cfg);

    if (!mci)
        std::fprintf(stderr, "[VertexPattleColor3D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
