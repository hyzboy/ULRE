#include"Build2DCommon.h"
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/log/Log.h>
#include<hgl/mtl/SamplerName.h>
#include "../common/VertexShaderAssembler.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredText2DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "Text2D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::Text2D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.usage_tag = MaterialDefinitionUsageTag::Text;
        bmi.bootstrap_kind = MaterialDefinitionBootstrapKind::TextAlphaBlend;
        bmi.ubo_requirements = {UBODescriptorSemantic::ViewportInfo};
        bmi.texture_slot_decls = {{TextureSlot::BaseColor, GLSLSamplerType::Sampler2D, true, SamplerName::Text}};
        bmi.ssbo_slot_decls = {{"mtl", SSBOType::TransmissionSurface}};
        bmi.vertex_node_config = Make2DNodeConfigOrtho(false);
        const MaterialVertexAttributeDefinition attrs[] = {
            {VertexSemantic::TexCoord, 1, VK_FORMAT_R32G32_SFLOAT, "layout(location=1) in vec2 TexCoord;\n"}
        };
        MaterialVertexVaryingConfig varying{};
        varying.emit_data_index_id = true;
        varying.emit_uv0 = true;
        ConfigureMaterialDefinitionContract(bmi, "2d/text2d.frag.glsl", nullptr,
                                             VK_FORMAT_R32G32_SINT, varying, attrs, 1);
        const GLSLCodeModuleSemanticRequirement vertex_requirements[] = {
            MakeMaterialVertexSemanticRequirement(VertexSemantic::Position),
            MakeMaterialVertexSemanticRequirement(VertexSemantic::TexCoord)
        };
        ConfigureMaterialVertexSemanticContract(
            bmi, vertex_requirements, 2, MaterialVertexProviderPolicy::GeometryOnly);
        bmi.fragment_program_mode = MaterialFragmentProgramMode::DirectInclude;
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::Text2D, bmi);
        MaterialDefinition alias = bmi;
        alias.definition_id = BUILTIN_MTL_DEF_TEXT;
        alias.definition_name = "builtin/text";
        RegisterMaterialDefinition(alias);

        return true;
    }();
}

void ForceLinkText2DMaterialDefinition() {}

}//namespace hgl::graph::mtl