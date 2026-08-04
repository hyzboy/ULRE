#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/common/RenderAssignDef.h>
#include<hgl/log/Log.h>
#include<vector>
#include "../common/VertexBuilderCommon.h"
#include "../common/VertexShaderAssembler.h"
#include "DefinitionDescriptorBuilder3D.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredVertexColor3DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "VertexColor3D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::VertexColor3D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.ubo_requirements  = {UBODescriptorSemantic::ViewportInfo, UBODescriptorSemantic::CameraInfo};
        bmi.vertex_node_config = MakeDefault3DNodeConfig();
        const MaterialVertexAttributeDefinition attributes[] = {
            {VertexSemantic::Position, 0, VK_FORMAT_R32G32B32_SFLOAT, nullptr},
            {VertexSemantic::Color, 1, VK_FORMAT_R32G32B32A32_SFLOAT, "layout(location=1) in vec4 Color;\n"}
        };
        const ShaderStageInterfaceVariable inputs[] = {
            {0, ShaderStageValueType::Vec3, 0, 0},
            {0, ShaderStageValueType::Vec4, 1, 0}
        };
        const ShaderStageInterfaceVariable outputs[] = {
            {0, ShaderStageValueType::Vec4, 0, 0}
        };
        MaterialVertexVaryingConfig varying{};
        varying.emit_vertex_color = true;
        ConfigureMaterialShaderContract(bmi, "compositor/main_forward_unlit_vertexcolor.frag.glsl",
                                         MaterialShaderDomain::World3D,
                                         MaterialFragmentProgramMode::Compositor,
                                         attributes, 2, inputs, 2, outputs, 1, varying);
        ConfigureMaterialVertexSemanticContractFromAttributes(bmi, attributes, 2, false);
        bmi.fragment_surface_module = "surface/unlit_vertexcolor_surface.glsl";
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::VertexColor3D, bmi);
        return true;
    }();
}// anonymous namespace

void ForceLinkVertexColor3DMaterialDefinition() {}

}//namespace hgl::graph::mtl