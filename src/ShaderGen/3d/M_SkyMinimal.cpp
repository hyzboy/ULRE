#include <hgl/shadergen/ShaderProgramBuildSpec.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/common/RenderAssignDef.h>
#include<hgl/log/Log.h>
#include<vector>
#include "../common/VertexBuilderCommon.h"
#include "../common/VertexShaderAssembler.h"
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
        bmi.ubo_requirements  = {UBODescriptorSemantic::ViewportInfo, UBODescriptorSemantic::CameraInfo, UBODescriptorSemantic::SkyInfo};
        bmi.vertex_node_config = MakeDefault3DNodeConfig();
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
    };

    CompositorAssembler assembler("ShaderLibrary");
    auto fs_result = assembler.Assemble(
        SurfaceType::Sky,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        "compositor/main_forward_sky.frag.glsl",
        "surface/sky_minimal_surface.glsl"
    );

    if (!fs_result.success)
    {
        GLogError("[SkyMinimal] CompositorAssembler failed: %s",
                  fs_result.error_message.c_str());
        return nullptr;
    }

    // emit_frag_direction: main_forward_sky.frag.glsl expects location=0 vec3 fragDirection.
    VertexVaryingConfig varying_cfg;
    varying_cfg.emit_frag_direction = true;
    std::string vs_glsl = GenerateVertexShader(
        definition.vertex_node_config,
        varying_cfg,
        VK_FORMAT_R32G32B32_SFLOAT,
        "",
        "ShaderLibrary"
    );

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        vs_glsl,
        fs_result.fragment_glsl,
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
