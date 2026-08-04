#include"Build2DCommon.h"
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/log/Log.h>
#include "../common/VertexShaderAssembler.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredVertexColor2DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "VertexColor2D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::VertexColor2D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.ubo_requirements = {UBODescriptorSemantic::ViewportInfo};
        bmi.vertex_node_config = Make2DNodeConfigNDC(true);
        const MaterialVertexAttributeDefinition attributes[] = {
            {VertexSemantic::Position, 0, VK_FORMAT_R32G32_SFLOAT, nullptr},
            {VertexSemantic::Color, 1, VK_FORMAT_R32G32B32A32_SFLOAT, "layout(location=1) in vec4 Color;\n"}
        };
        const ShaderStageInterfaceVariable inputs[] = {
            {0, ShaderStageValueType::Vec2, 0, 0},
            {0, ShaderStageValueType::Vec4, 1, 0}
        };
        const ShaderStageInterfaceVariable outputs[] = {
            {0, ShaderStageValueType::Vec4, 0, 0}
        };
        MaterialVertexVaryingConfig varying{};
        varying.emit_vertex_color = true;
        ConfigureMaterialShaderContract(bmi, "2d/vertexcolor2d.frag.glsl",
                                         MaterialShaderDomain::Screen2D,
                                         MaterialFragmentProgramMode::DirectInclude,
                                         attributes, 2, inputs, 2, outputs, 1, varying);
        ConfigureMaterialVertexSemanticContractFromAttributes(bmi, attributes, 2, false);
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::VertexColor2D, bmi);
        return true;
    }();
}

void ForceLinkVertexColor2DMaterialDefinition() {}

}//namespace hgl::graph::mtl