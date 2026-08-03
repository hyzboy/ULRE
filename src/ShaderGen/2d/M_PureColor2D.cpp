#include"Build2DCommon.h"
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/log/Log.h>
#include "../common/VertexShaderAssembler.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredPureColor2DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "PureColor2D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::PureColor2D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.ubo_requirements = {UBODescriptorSemantic::ViewportInfo};
        bmi.usage_tag   = MaterialDefinitionUsageTag::Fallback;
        bmi.ssbo_slot_decls = {{"mtl", SSBOType::EmissiveSurface}};
        bmi.vertex_node_config = Make2DNodeConfigNDC(true);
        ConfigureSimpleMaterialShaderContract(bmi, "2d/purecolor2d.frag.glsl", true, false, false, false);
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::PureColor2D, bmi);
        // Register builtin alias so fallback code can look up by canonical id.
        MaterialDefinition fallback_alias = bmi;
        fallback_alias.definition_id = BUILTIN_MTL_DEF_FALLBACK_2D;
        fallback_alias.definition_name = "builtin/fallback_2d";
        RegisterMaterialDefinition(fallback_alias);

        return true;
    }();
}

void ForceLinkPureColor2DMaterialDefinition() {}

}//namespace hgl::graph::mtl