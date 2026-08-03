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
        ConfigureSimpleMaterialShaderContract(bmi, "2d/puretexture2d.frag.glsl", false, true, false, false);
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::PureTexture2D, bmi);
        return true;
    }();
}

void ForceLinkPureTexture2DMaterialDefinition() {}

}//namespace hgl::graph::mtl