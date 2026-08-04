#include <hgl/shadergen/ShaderProgramBuildSpec.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/common/RenderAssignDef.h>
#include<hgl/log/Log.h>
#include<vector>
#include "../common/VertexBuilderCommon.h"
#include "../common/VertexShaderAssembler.h"
#include "DefinitionDescriptorBuilder3D.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredSkyMinimalBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "SkyMinimal";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::SkyMinimal);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.usage_tag = MaterialDefinitionUsageTag::Sky;
        bmi.ubo_requirements  = {UBODescriptorSemantic::ViewportInfo, UBODescriptorSemantic::CameraInfo, UBODescriptorSemantic::SkyInfo};
        bmi.vertex_node_config = MakeDefault3DNodeConfig();
        MaterialVertexVaryingConfig varying{};
        varying.emit_frag_direction = true;
        ConfigureMaterialDefinitionContract(bmi, "compositor/main_forward_sky.frag.glsl",
                                             "surface/sky_minimal_surface.glsl",
                                             VK_FORMAT_R32G32B32_SFLOAT, varying, nullptr, 0);
        ConfigureMaterialVertexSemanticContractFromAttributes(bmi, nullptr, 0, true);
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::SkyMinimal, bmi);
        MaterialDefinition alias = bmi;
        alias.definition_id = BUILTIN_MTL_DEF_SKY;
        alias.definition_name = "builtin/sky";
        RegisterMaterialDefinition(alias);

        return true;
    }();

}//namespace

void ForceLinkSkyMinimalMaterialDefinition() {}

}//namespace hgl::graph::mtl