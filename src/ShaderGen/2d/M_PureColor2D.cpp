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
        bmi.bootstrap_kind = MaterialDefinitionBootstrapKind::PureColor;
        bmi.ssbo_slot_decls = {{"mtl", SSBOType::EmissiveSurface}};
        bmi.vertex_node_config = Make2DNodeConfigNDC(true);
        const MaterialVertexAttributeDefinition attributes[] = {
            {VertexSemantic::Position, 0, VK_FORMAT_R32G32_SFLOAT, nullptr}
        };
        const ShaderStageInterfaceVariable inputs[] = {
            {0, ShaderStageValueType::Vec2, 0, 0}
        };
        const ShaderStageInterfaceVariable outputs[] = {
            {0, ShaderStageValueType::UInt, 0, uint32(ShaderStageInterfaceFlags::Flat)}
        };
        MaterialVertexVaryingConfig varying{};
        varying.emit_data_index_id = true;
        ConfigureMaterialShaderContract(bmi, "2d/purecolor2d.frag.glsl",
                                         MaterialShaderDomain::Screen2D,
                                         MaterialFragmentProgramMode::DirectInclude,
                                         attributes, 1, inputs, 1, outputs, 1, varying);
        const GLSLCodeModuleSemanticRequirement vertex_requirements[] = {
            MakeMaterialVertexSemanticRequirement(VertexSemantic::Position)
        };
        ConfigureMaterialVertexSemanticContract(
            bmi, vertex_requirements, 1, MaterialVertexProviderPolicy::GeometryOnly);
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