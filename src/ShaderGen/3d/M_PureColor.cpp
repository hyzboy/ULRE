#include <hgl/shadergen/ShaderProgramBuildSpec.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/log/Log.h>

namespace hgl::graph::mtl
{
    namespace
    {
        const bool kRegisteredPureColorBmi = []() -> bool
        {
            MaterialDefinition bmi{};
            bmi.definition_id = BUILTIN_MTL_DEF_PURE_COLOR;
            bmi.definition_name = "builtin/pure_color";
            bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::PureColor);
            bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
            bmi.usage_tag = MaterialDefinitionUsageTag::Fallback;
            bmi.bootstrap_kind = MaterialDefinitionBootstrapKind::PureColor;
            bmi.data_slot_decls = {{"mtl", SSBOType::EmissiveSurface}};
            bmi.ubo_requirements = {
                UBODescriptorSemantic::ViewportInfo,
                UBODescriptorSemantic::CameraInfo
            };
            bmi.vertex_node_config = MakeDefault3DNodeConfig();
            SetMaterialFragmentSource(bmi, "compositor/pure_color.frag.glsl");
            bmi.fragment_program_mode = MaterialFragmentProgramMode::Compositor;
            bmi.fragment_surface_module = nullptr;
            bmi.vertex_varying.emit_data_index_id = true;

            const GLSLCodeModuleSemanticRequirement requirements[] = {
                MakeMaterialVertexSemanticRequirement(VertexSemantic::Position)
            };
            ConfigureMaterialVertexSemanticContract(
                bmi, requirements, 1, MaterialVertexProviderPolicy::GeometryOnly);
            RegisterMaterialDefinition(BuiltinMaterialCreatorID::PureColor, bmi);
            RegisterMaterialDefinitionAlias(
                   BUILTIN_MTL_DEF_MISSING_MATERIAL,
                   BUILTIN_MTL_DEF_PURE_COLOR);
            return true;
        }();
    }

    void ForceLinkPureColorMaterialDefinition() {}
}
