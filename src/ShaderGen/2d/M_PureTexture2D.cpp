#include"Build2DCommon.h"
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/log/Log.h>
#include "../common/VertexShaderAssembler.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredPureTexture2DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "PureTexture2D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::PureTexture2D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.ubo_requirements = {UBODescriptorSemantic::ViewportInfo};
        bmi.texture_slot_decls = {{TextureSlot::BaseColor, GLSLSamplerType::Sampler2D, true}};
        bmi.vertex_node_config = Make2DNodeConfigNDC(true);
        bmi.shader_domain = MaterialShaderDomain::Screen2D;
        const MaterialVertexAttributeDefinition attributes[] = {
            {VertexSemantic::Position, 0, VK_FORMAT_R32G32_SFLOAT, nullptr},
            {VertexSemantic::TexCoord, 1, VK_FORMAT_R32G32_SFLOAT, "layout(location=1) in vec2 TexCoord;\n"}
        };
        const ShaderStageInterfaceVariable inputs[] = {
            {0, ShaderStageValueType::Vec2, 0, 0},
            {0, ShaderStageValueType::Vec2, 1, 0}
        };
        const ShaderStageInterfaceVariable outputs[] = {
            {0, ShaderStageValueType::Vec2, 0, 0}
        };
        MaterialVertexVaryingConfig varying{};
        varying.emit_uv0 = true;
        ConfigureMaterialShaderContract(bmi, "2d/puretexture2d.frag.glsl",
                                         MaterialShaderDomain::Screen2D,
                                         MaterialFragmentProgramMode::DirectInclude,
                                         attributes, 2, inputs, 2, outputs, 1, varying);
        ConfigureMaterialVertexSemanticContractFromAttributes(bmi, attributes, 2, false);
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::PureTexture2D, bmi);
        return true;
    }();
}

void ForceLinkPureTexture2DMaterialDefinition() {}

}//namespace hgl::graph::mtl