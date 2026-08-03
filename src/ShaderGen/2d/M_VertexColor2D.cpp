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
        ConfigureSimpleMaterialShaderContract(bmi, "2d/vertexcolor2d.frag.glsl", false, false, true, false);
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::VertexColor2D, bmi);
        return true;
    }();
}

void ForceLinkVertexColor2DMaterialDefinition() {}

}//namespace hgl::graph::mtl