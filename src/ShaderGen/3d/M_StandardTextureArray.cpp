#include <hgl/shadergen/ShaderProgramBuildSpec.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/common/RenderAssignDef.h>
#include <hgl/log/Log.h>
#include <vector>

#include "../common/MFSkyLight.h"
#include "../common/VertexBuilderCommon.h"
#include "../common/VertexShaderAssembler.h"
#include "DefinitionDescriptorBuilder3D.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredStandardTextureArrayBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "StandardTextureArray";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::StandardTextureArray);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.ssbo_slot_decls   = {{"mtl", SSBOType::ClearCoatSurface}};
        bmi.ubo_requirements  = {UBODescriptorSemantic::ViewportInfo, UBODescriptorSemantic::CameraInfo, UBODescriptorSemantic::SkyInfo};
        bmi.texture_slot_decls = {
            {TextureSlot::BaseColor, GLSLSamplerType::Sampler2DArray, false},
            {TextureSlot::Normal,    GLSLSamplerType::Sampler2DArray, false},
            {TextureSlot::Roughness, GLSLSamplerType::Sampler2DArray, false},
        };
        bmi.vertex_node_config = MakeDefault3DNodeConfig();
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::StandardTextureArray, bmi);
        return true;
    }();

}

static ShaderProgramBuildSpec *CreateStandardTextureArrayImpl(const contract::PhysicalDeviceProfileLite *profile, CompositorMaterialBuildConfig bc, const MaterialDefinition &definition)
{
    SkyLightAmbientModel ambient = bc.sky_ambient_model;

    std::vector<FixedDescriptorEntry> dynamic_descriptors = Build3DDescriptorsFromDefinition(definition);

    std::vector<const char *> unused_resources;
    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(ambient),
        dynamic_descriptors,
        unused_resources);

    const vertex_builder_common::VertexSemanticDecl vertex_decls[] = {
        { VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT },
        { VertexSemantic::TexCoord, VK_FORMAT_R32G32_SFLOAT },
        { VertexSemantic::Normal,   VK_FORMAT_R32G32B32_SFLOAT },
    };
    const vertex_builder_common::VertexBuildInput vertex_input {
        PrimitiveType::Triangles,
        bc.geometry_vertex_format,
        vertex_decls,
        3
    };
    std::vector<FixedVertexEntry> standard_array_vertex = vertex_builder_common::BuildVertexEntries(vertex_input);

    FixedMaterialDef dynamic_def {
        "StandardTextureArray_v1",
        PrimitiveType::Triangles,
        standard_array_vertex.data(),
        uint32_t(standard_array_vertex.size()),
        dynamic_descriptors.data(),
        uint32_t(dynamic_descriptors.size()),
    };

    CompositorAssembler assembler("ShaderLibrary");

    auto fs_result = assembler.Assemble(
        SurfaceType::Standard,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        "compositor/main_forward_lit.frag.glsl",
        "surface/standard_texturearray_surface.glsl"
    );

    if (!fs_result.success)
    {
        GLogError("[StandardTextureArray] CompositorAssembler failed: %s",
                  fs_result.error_message.c_str());
        return nullptr;
    }

    VertexVaryingConfig varying_cfg;
    varying_cfg.emit_data_index_id    = true;
    varying_cfg.emit_texture_layer_id = true;
    varying_cfg.texture_layer_id_uses_data_index = true;
    varying_cfg.emit_world_pos        = true;
    varying_cfg.emit_world_normal     = true;
    varying_cfg.emit_uv0              = true;
    const std::string extra_attrs =
        "layout(location=1) in vec2 TexCoord;\n"
        "layout(location=2) in vec3 Normal;\n";
    std::string vs_glsl = GenerateVertexShader(
        definition.vertex_node_config,
        varying_cfg,
        VK_FORMAT_R32G32B32_SFLOAT,
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
        GLogError("[StandardTextureArray] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreateStandardTextureArray(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreateStandardTextureArrayImpl(profile, ToCompositorBuildConfig3D(request, definition), definition);
}
}//namespace hgl::graph::mtl
