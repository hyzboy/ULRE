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
    const bool kRegisteredVertexLuminance3DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "VertexLuminance3D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::VertexLuminance3D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.ssbo_slot_decls   = {{"mtl", SSBOType::EmissiveSurface}};
        bmi.ubo_requirements  = {UBODescriptorSemantic::ViewportInfo, UBODescriptorSemantic::CameraInfo};
        bmi.vertex_node_config = MakeDefault3DNodeConfig();
        const MaterialVertexAttributeDefinition attrs[] = {
            {VertexSemantic::Luminance, 1, VK_FORMAT_R32_SFLOAT, "layout(location=1) in float Luminance;\n"}
        };
        MaterialVertexVaryingConfig varying{};
        varying.emit_data_index_id = true;
        varying.emit_texture_layer_id = true;
        varying.texture_layer_id_uses_data_index = true;
        varying.emit_luminance = true;
        ConfigureMaterialDefinitionContract(bmi, "compositor/main_forward_unlit_luminance.frag.glsl",
                                             "surface/unlit_luminance_surface.glsl",
                                             VK_FORMAT_R32G32B32_SFLOAT, varying, attrs, 1);
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::VertexLuminance3D, bmi);
        return true;
    }();

}

void ForceLinkVertexLuminance3DMaterialDefinition() {}

}//namespace hgl::graph::mtl