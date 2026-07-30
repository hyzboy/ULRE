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

#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/common/RenderAssignDef.h>
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredVertexPattleColor3DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "VertexPattleColor3D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::VertexPattleColor3D);
        bmi.source_kind = BMISourceKind::BuiltIn;
        bmi.with_camera       = true;
        bmi.with_local_to_world = true;
        bmi.with_sky          = false;
        RegisterBaseMaterialInfo(BuiltinMaterialCreatorID::VertexPattleColor3D, bmi);
        return true;
    }();

    constexpr FixedDescriptorEntry VERTEX_PATTLE_COLOR_3D_DESCRIPTORS[] = {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::UBO},
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr, DescriptorSemantic::CameraInfo, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::UBO},
        { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr, DescriptorSemantic::LocalToWorld, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, GetDescriptorSemanticLayerByKind(TransformDescriptorKind) },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO},
        { DescriptorSetType::Material, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "color_pattle", "ColorPattle", nullptr, DescriptorSemantic::MaterialColorPalette, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::UBO },
    };

}//namespace

ShaderProgramBuildSpec *CreateVertexPattleColor3D(const contract::PhysicalDeviceProfileLite *profile,const Material3DCreateConfig *cfg)
{
    Material3DCreateConfig local_cfg = cfg ? *cfg : Material3DCreateConfig();

    static const ShaderBufferSource * const private_sbs_list[] =
    {
        &SBS_ColorPattle
    };
    local_cfg.SetPrivateShaderBufferSources(private_sbs_list,1);

    FixedVertexEntry vertex_pattle_color_3d_vertex[] = {
        { ResolveMaterialVertexSemanticFormat(&local_cfg, VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT), VertexSemantic::Position },
        { ResolveMaterialVertexSemanticFormat(&local_cfg, VertexSemantic::Color,    VK_FORMAT_R32_UINT),          VertexSemantic::Color },
        { Assign::TransformID::VAB_FMT,  Assign::TransformID::VIS_SEMANTIC },
    };

    FixedMaterialDef dynamic_def {
        "VertexPattleColor3D",
        PrimitiveType::Triangles,
        vertex_pattle_color_3d_vertex,
        uint32_t(sizeof(vertex_pattle_color_3d_vertex) / sizeof(vertex_pattle_color_3d_vertex[0])),
        VERTEX_PATTLE_COLOR_3D_DESCRIPTORS,
        uint32_t(sizeof(VERTEX_PATTLE_COLOR_3D_DESCRIPTORS) / sizeof(VERTEX_PATTLE_COLOR_3D_DESCRIPTORS[0])),
        nullptr,
        0,
    };

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

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        result.vertex_glsl,
        result.fragment_glsl,
        &local_cfg);

    if (!mci)
        std::fprintf(stderr, "[VertexPattleColor3D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl


