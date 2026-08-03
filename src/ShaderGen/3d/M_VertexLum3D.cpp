#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/common/RenderAssignDef.h>
#include<hgl/log/Log.h>
#include<vector>
#include "../common/VertexBuilderCommon.h"
#include "../common/VertexShaderAssembler.h"
#include "DefinitionDescriptorBuilder3D.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredVertexLuminance3DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "VertexLuminance3D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::VertexLuminance3D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.ssbo_slot_decls   = {{"mtl", SSBOType::EmissiveSurface}};
        bmi.ubo_requirements  = {UBODescriptorSemantic::ViewportInfo, UBODescriptorSemantic::CameraInfo};
        bmi.vertex_node_config = MakeDefault3DNodeConfig();
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::VertexLuminance3D, bmi);
        return true;
    }();

}

static ShaderProgramBuildSpec *CreateVertexLuminance3DImpl(const contract::PhysicalDeviceProfileLite *profile, CompositorMaterialBuildConfig bc, const MaterialDefinition &definition)
{
    std::vector<FixedDescriptorEntry> dynamic_descriptors = Build3DDescriptorsFromDefinition(definition);
    const vertex_builder_common::LuminanceVertexBuildResult vertex_result =
        vertex_builder_common::BuildLuminanceVertexEntries(bc.geometry_vertex_format);

    FixedMaterialDef dynamic_def {
        "VertexLuminance3D",
        PrimitiveType::Triangles,
        vertex_result.entries.data(),
        uint32_t(vertex_result.entries.size()),
        dynamic_descriptors.data(),
        uint32_t(dynamic_descriptors.size()),
    };

    CompositorAssembler assembler("ShaderLibrary");
    auto fs_result = assembler.Assemble(
        SurfaceType::Unlit,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        "compositor/main_forward_unlit_luminance.frag.glsl",
        "surface/unlit_luminance_surface.glsl"
    );

    if (!fs_result.success)
    {
        GLogError("[VertexLuminance3D] CompositorAssembler failed: %s",
                  fs_result.error_message.c_str());
        return nullptr;
    }

    // emit_data_index_id + emit_texture_layer_id(uses_data_index) + emit_luminance:
    // matches main_forward_unlit_luminance.frag.glsl (loc0/1/2).
    VertexVaryingConfig varying_cfg;
    varying_cfg.emit_data_index_id               = true;
    varying_cfg.emit_texture_layer_id            = true;
    varying_cfg.texture_layer_id_uses_data_index = true;
    varying_cfg.emit_luminance                   = true;

    VertexShaderNodeConfig node_cfg = definition.vertex_node_config;
    if (vertex_result.use_vec2_position)
        node_cfg.position_mapping = PositionMappingMode::LiftXY_XY0;

    const std::string extra_attrs = "layout(location=1) in float Luminance;\n";
    std::string vs_glsl = GenerateVertexShader(
        node_cfg,
        varying_cfg,
        vertex_result.use_vec2_position ? VK_FORMAT_R32G32_SFLOAT : VK_FORMAT_R32G32B32_SFLOAT,
        extra_attrs,
        "ShaderLibrary"
    );

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        vs_glsl,
        fs_result.fragment_glsl,
        bc);

    if (!mci)
        GLogError("[VertexLuminance3D] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreateVertexLuminance3D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreateVertexLuminance3DImpl(profile, ToCompositorBuildConfig3D(request, definition), definition);
}
}//namespace hgl::graph::mtl
