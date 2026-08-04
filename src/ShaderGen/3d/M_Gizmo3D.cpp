#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/common/RenderAssignDef.h>
#include<hgl/log/Log.h>
#include<vector>
#include "../common/VertexBuilderCommon.h"
#include "../common/VertexShaderAssembler.h"
#include "DefinitionDescriptorBuilder3D.h"

namespace hgl::graph::mtl
{
namespace
{
    const bool kRegisteredGizmo3DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "Gizmo3D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::Gizmo3D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.ssbo_slot_decls   = {{"mtl", SSBOType::EmissiveSurface}};
        bmi.ubo_requirements  = {UBODescriptorSemantic::ViewportInfo, UBODescriptorSemantic::CameraInfo};
        bmi.vertex_node_config = MakeDefault3DNodeConfig();
        bmi.shader_domain = MaterialShaderDomain::World3D;
        bmi.compositor_surface = SurfaceType::Unlit;
        bmi.compositor_blend = BlendMode::Opaque;
        bmi.compositor_pass = PassType::ForwardOpaque;
        const MaterialVertexAttributeDefinition attrs[] = {
            {VertexSemantic::Normal, 1, VK_FORMAT_R32G32B32_SFLOAT, "layout(location=1) in vec3 Normal;\n"}
        };
        MaterialVertexVaryingConfig varying{};
        varying.emit_data_index_id = true;
        varying.emit_texture_layer_id = true;
        varying.texture_layer_id_uses_data_index = true;
        varying.emit_world_pos = true;
        varying.emit_world_normal = true;
        ConfigureMaterialDefinitionContract(bmi, "compositor/main_forward_unlit_normal.frag.glsl",
                                             "surface/gizmo3d_surface.glsl",
                                             VK_FORMAT_R32G32B32_SFLOAT, varying, attrs, 1);
        ConfigureMaterialVertexSemanticContractFromAttributes(bmi, attrs, 1, true);
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::Gizmo3D, bmi);
        return true;
    }();

}

void ForceLinkGizmo3DMaterialDefinition() {}

}//namespace hgl::graph::mtl