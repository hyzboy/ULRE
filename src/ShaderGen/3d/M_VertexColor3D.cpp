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
        ConfigureSimpleMaterialShaderContract(bmi, "compositor/main_forward_unlit_vertexcolor.frag.glsl", false, false, true, true);
        bmi.fragment_surface_module = "surface/unlit_vertexcolor_surface.glsl";
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::VertexColor3D, bmi);
        return true;
    }();
}// anonymous namespace

void ForceLinkVertexColor3DMaterialDefinition() {}

}//namespace hgl::graph::mtl