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
    const bool kRegisteredPureColor3DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "PureColor3D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::PureColor3D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.usage_tag   = MaterialDefinitionUsageTag::Fallback;
        bmi.ssbo_slot_decls   = {{"mtl", SSBOType::EmissiveSurface}};
        bmi.ubo_requirements  = {UBODescriptorSemantic::ViewportInfo, UBODescriptorSemantic::CameraInfo};
        bmi.vertex_node_config = MakeDefault3DNodeConfig();
        ConfigureSimpleMaterialShaderContract(bmi, "compositor/main_forward_unlit.frag.glsl", true, false, false, true, true);
        bmi.fragment_surface_module = "surface/unlit_color3d_surface.glsl";
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::PureColor3D, bmi);
        // builtin/fallback_3d: 3D 无材质保底
        {
            MaterialDefinition alias = bmi;
            alias.definition_id = BUILTIN_MTL_DEF_FALLBACK_3D;
            alias.definition_name = "builtin/fallback_3d";
            RegisterMaterialDefinition(alias);
        }

        // builtin/missing_material: 缺失材质（当前与 fallback_3d 相同，未来可换成棋盘格）
        {
            MaterialDefinition alias = bmi;
            alias.definition_id = BUILTIN_MTL_DEF_MISSING_MATERIAL;
            alias.definition_name = "builtin/missing_material";
            RegisterMaterialDefinition(alias);
        }

        return true;
    }();

}

void ForceLinkPureColor3DMaterialDefinition() {}

}//namespace hgl::graph::mtl