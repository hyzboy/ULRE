#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/common/RenderAssignDef.h>
#include<cstdio>
#include<string>

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredVertexColor3DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.bmi_name = "VertexColor3D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::VertexColor3D);
        bmi.source_kind = BMISourceKind::BuiltIn;
        bmi.with_camera       = true;
        bmi.with_local_to_world = true;
        bmi.with_sky          = false;
        RegisterBaseMaterialInfo(BuiltinMaterialCreatorID::VertexColor3D, bmi);
        return true;
    }();

    constexpr FixedDescriptorEntry VERTEX_COLOR_3D_DESCRIPTORS[] = {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::UBO},
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr, DescriptorSemantic::CameraInfo, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::UBO},
        { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr, DescriptorSemantic::LocalToWorld, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, GetDescriptorSemanticLayerByKind(TransformDescriptorKind) },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO},
    };
}

ShaderProgramBuildSpec *CreateVertexColor3D(const contract::PhysicalDeviceProfileLite *profile,const Material3DCreateConfig *cfg)
{
    FixedVertexEntry vertex_color_3d_vertex[] = {
        { ResolveMaterialVertexSemanticFormat(cfg, VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT),    VertexSemantic::Position },
        { ResolveMaterialVertexSemanticFormat(cfg, VertexSemantic::Color,    VK_FORMAT_R32G32B32A32_SFLOAT), VertexSemantic::Color },
    };

    FixedMaterialDef dynamic_def {
        "VertexColor3D",
        PrimitiveType::Triangles,
        vertex_color_3d_vertex,
        uint32_t(sizeof(vertex_color_3d_vertex) / sizeof(vertex_color_3d_vertex[0])),
        VERTEX_COLOR_3D_DESCRIPTORS,
        uint32_t(sizeof(VERTEX_COLOR_3D_DESCRIPTORS) / sizeof(VERTEX_COLOR_3D_DESCRIPTORS[0])),
        nullptr,
        0,
    };

    // 通过 CompositorAssembler 从 .glsl 模板文件组装 VS/FS
    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(
        SurfaceType::Unlit,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        "compositor/main_forward_unlit_vertexcolor.vert.glsl",  // VS: Pos+Color+TID
        "compositor/main_forward_unlit_vertexcolor.frag.glsl",  // FS: vertexColor, 无 MI
        "surface/unlit_vertexcolor_surface.glsl"                // Surface: 顶点色直通
    );

    if (!result.success)
    {
        std::fprintf(stderr, "[VertexColor3D] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[VertexColor3D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl

