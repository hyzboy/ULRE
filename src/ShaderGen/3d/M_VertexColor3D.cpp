#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/common/RenderAssignDef.h>
#include<hgl/log/Log.h>
#include<vector>
#include "DefinitionDescriptorBuilder3D.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredVertexColor3DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "VertexColor3D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::VertexColor3D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.with_camera       = true;
        bmi.with_local_to_world = true;
        bmi.with_sky          = false;
        bmi.ubo_requirements  = {UBODescriptorSemantic::ViewportInfo, UBODescriptorSemantic::CameraInfo};
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::VertexColor3D, bmi);
        return true;
    }();
}// anonymous namespace

static ShaderProgramBuildSpec *CreateVertexColor3DImpl(const contract::PhysicalDeviceProfileLite *profile, const CompositorMaterialBuildConfig &bc, const MaterialDefinition &definition)
{
    std::vector<FixedDescriptorEntry> dynamic_descriptors = Build3DDescriptorsFromDefinition(definition);

    FixedVertexEntry vertex_color_3d_vertex[] = {
        { ResolveMaterialVertexSemanticFormat(bc.geometry_vertex_format, VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT),    VertexSemantic::Position },
        { ResolveMaterialVertexSemanticFormat(bc.geometry_vertex_format, VertexSemantic::Color,    VK_FORMAT_R32G32B32A32_SFLOAT), VertexSemantic::Color },
    };

    FixedMaterialDef dynamic_def {
        "VertexColor3D",
        PrimitiveType::Triangles,
        vertex_color_3d_vertex,
        uint32_t(sizeof(vertex_color_3d_vertex) / sizeof(vertex_color_3d_vertex[0])),
        dynamic_descriptors.data(),
        uint32_t(dynamic_descriptors.size()),
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
        "compositor/main_forward_unlit_vertexcolor.vert.glsl",
        "compositor/main_forward_unlit_vertexcolor.frag.glsl",
        "surface/unlit_vertexcolor_surface.glsl"
    );

    if (!result.success)
    {
        GLogError("[VertexColor3D] CompositorAssembler failed: %s",
                  result.error_message.c_str());
        return nullptr;
    }

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        result.vertex_glsl,
        result.fragment_glsl,
        bc);

    if (!mci)
        GLogError("[VertexColor3D] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreateVertexColor3D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreateVertexColor3DImpl(profile, ToCompositorBuildConfig3D(request, definition), definition);
}
}//namespace hgl::graph::mtl
