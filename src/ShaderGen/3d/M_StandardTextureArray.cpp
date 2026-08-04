#include <hgl/shadergen/ShaderProgramBuildSpec.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/common/RenderAssignDef.h>
#include <hgl/log/Log.h>
#include <vector>

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
        bmi.code_module_requirements = {GLSLCodeModuleID::SkyLightHeader, GLSLCodeModuleID::PBRSurface};
        bmi.texture_slot_decls = {
            {TextureSlot::BaseColor, GLSLSamplerType::Sampler2DArray, false},
            {TextureSlot::Normal,    GLSLSamplerType::Sampler2DArray, false},
            {TextureSlot::Metallic,  GLSLSamplerType::Sampler2DArray, false},
            {TextureSlot::Roughness, GLSLSamplerType::Sampler2DArray, false},
            {TextureSlot::Occlusion, GLSLSamplerType::Sampler2DArray, false},
        };
        bmi.vertex_node_config = MakeDefault3DNodeConfig();
        bmi.shader_domain = MaterialShaderDomain::World3D;
        bmi.compositor_surface = SurfaceType::Standard;
        bmi.compositor_blend = BlendMode::Opaque;
        bmi.compositor_pass = PassType::ForwardOpaque;
        const MaterialVertexAttributeDefinition attrs[] = {
            {VertexSemantic::TexCoord, 1, VK_FORMAT_R32G32_SFLOAT, "layout(location=1) in vec2 TexCoord;\n"},
            {VertexSemantic::Normal, 2, VK_FORMAT_R32G32B32_SFLOAT, "layout(location=2) in vec3 Normal;\n"}
        };
        MaterialVertexVaryingConfig varying{};
        varying.emit_data_index_id = true;
        varying.emit_texture_layer_id = true;
        varying.texture_layer_id_uses_data_index = true;
        varying.emit_world_pos = true;
        varying.emit_world_normal = true;
        varying.emit_uv0 = true;
        ConfigureMaterialDefinitionContract(bmi, "compositor/main_forward_lit.frag.glsl",
                                             "surface/standard_texturearray_surface.glsl",
                                             VK_FORMAT_R32G32B32_SFLOAT, varying, attrs, 2);
        ConfigureMaterialVertexSemanticContractFromAttributes(bmi, attrs, 2, true);
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::StandardTextureArray, bmi);
        return true;
    }();

}

void ForceLinkStandardTextureArrayMaterialDefinition() {}

}//namespace hgl::graph::mtl