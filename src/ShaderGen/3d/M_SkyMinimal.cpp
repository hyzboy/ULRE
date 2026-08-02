#include <hgl/shadergen/ShaderProgramBuildSpec.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/common/RenderAssignDef.h>
#include<hgl/log/Log.h>
#include<vector>
#include "../common/VertexBuilderCommon.h"
#include "DefinitionDescriptorBuilder3D.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredSkyMinimalBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "SkyMinimal";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::SkyMinimal);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.usage_tag = MaterialDefinitionUsageTag::Sky;
        bmi.with_camera       = true;
        bmi.with_local_to_world = true;
        bmi.with_sky          = true;
        bmi.ubo_requirements  = {UBODescriptorSemantic::ViewportInfo, UBODescriptorSemantic::CameraInfo, UBODescriptorSemantic::SkyInfo};
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::SkyMinimal, bmi);

        MaterialDefinition alias = bmi;
        alias.definition_id = BUILTIN_MTL_DEF_SKY;
        alias.definition_name = "builtin/sky";
        RegisterMaterialDefinition(alias);

        return true;
    }();

}//namespace

static ShaderProgramBuildSpec *CreateSkyMinimalImpl(const contract::PhysicalDeviceProfileLite *profile, const CompositorMaterialBuildConfig &bc, const MaterialDefinition &definition)
{
    Build3DDescriptorOptions options{};
    options.sky_stage_flags = uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT);
    std::vector<FixedDescriptorEntry> dynamic_descriptors = Build3DDescriptorsFromDefinition(definition, options);

    const vertex_builder_common::VertexSemanticDecl vertex_decls[] = {
        { VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT },
    };
    const vertex_builder_common::VertexBuildInput vertex_input {
        PrimitiveType::Triangles,
        vertex_builder_common::VertexTransformIntent::LocalToWorld,
        bc.geometry_vertex_format,
        vertex_decls,
        1
    };
    std::vector<FixedVertexEntry> sky_minimal_vertex = vertex_builder_common::BuildVertexEntries(vertex_input);

    FixedMaterialDef dynamic_def {
        "SkyMinimal",
        PrimitiveType::Triangles,
        sky_minimal_vertex.data(),
        uint32_t(sky_minimal_vertex.size()),
        dynamic_descriptors.data(),
        uint32_t(dynamic_descriptors.size()),
        nullptr,
        0,
    };

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(
        SurfaceType::Sky,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        "compositor/main_forward_sky.vert.glsl",
        "compositor/main_forward_sky.frag.glsl",
        "surface/sky_minimal_surface.glsl"
    );

    if (!result.success)
    {
        GLogError("[SkyMinimal] CompositorAssembler failed: %s",
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
        GLogError("[SkyMinimal] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreateSkyMinimal(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreateSkyMinimalImpl(profile, ToCompositorBuildConfig3D(request, definition), definition);
}
}//namespace hgl::graph::mtl
