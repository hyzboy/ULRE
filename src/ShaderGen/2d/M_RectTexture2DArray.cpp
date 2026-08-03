#include"Build2DCommon.h"
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/log/Log.h>
#include "../common/VertexShaderAssembler.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredRectTexture2DArrayBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "RectTexture2DArray";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::RectTexture2DArray);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.ubo_requirements = {UBODescriptorSemantic::ViewportInfo};
        bmi.texture_slot_decls = {{TextureSlot::BaseColor, GLSLSamplerType::Sampler2DArray, true}};
        bmi.ssbo_slot_decls = {{"mtl", SSBOType::TextureRectArraySurface}};
        bmi.vertex_node_config = Make2DNodeConfigNDC(true);
        const MaterialVertexAttributeDefinition attrs[] = {
            {VertexSemantic::TexCoord, 1, VK_FORMAT_R32G32_SFLOAT, "layout(location=1) in vec2 TexCoord;\n"}
        };
        MaterialVertexVaryingConfig varying{};
        varying.emit_data_index_id = true;
        varying.emit_uv0 = true;
        ConfigureMaterialDefinitionContract(bmi, "2d/recttexture2darray.frag.glsl", nullptr,
                                             VK_FORMAT_R32G32_SFLOAT, varying, attrs, 1);
        bmi.fragment_program_mode = MaterialFragmentProgramMode::DirectInclude;
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::RectTexture2DArray, bmi);
        return true;
    }();
}

void ForceLinkRectTexture2DArrayMaterialDefinition() {}

}//namespace hgl::graph::mtl