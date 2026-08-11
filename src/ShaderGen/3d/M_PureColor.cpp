#include <hgl/mtl/MaterialDefinitionRegistry.h>

namespace hgl::graph::mtl
{
    namespace
    {
        const bool kRegisteredPureColorBmi = []() -> bool
        {
            MaterialDefinition bmi{};
            bmi.definition_id = BUILTIN_MTL_DEF_PURE_COLOR;
            bmi.definition_name = "builtin/pure_color";
            bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
            bmi.usage_tag = MaterialDefinitionUsageTag::Fallback;
            bmi.bootstrap_kind = MaterialDefinitionBootstrapKind::PureColor;
            bmi.data_slot_decls = {{"mtl", SSBOType::EmissiveSurface}};
            bmi.ubo_requirements = {
                UBODescriptorSemantic::ViewportInfo,
                UBODescriptorSemantic::CameraInfo
            };
            bmi.vertex_node_config = MakeDefault3DNodeConfig();
            SetMaterialFragmentSource(
                bmi, "compositor/main_forward_surface.frag.glsl");
            bmi.fragment_surface_module =
                "surface/material_surface.glsl";
            bmi.fragment_material_source_module =
                "material/unlit_source.glsl";
            bmi.vertex_varying.emit_data_index_id = true;

            const GLSLCodeModuleSemanticRequirement requirements[] = {
                MakeMaterialVertexSemanticRequirement(VertexSemantic::Position)
            };
            ConfigureMaterialVertexSemanticContract(
                bmi, requirements, 1, MaterialVertexProviderPolicy::GeometryOnly);
            RegisterMaterialDefinition(bmi);
            RegisterMaterialDefinitionAlias(
                   BUILTIN_MTL_DEF_MISSING_MATERIAL,
                   BUILTIN_MTL_DEF_PURE_COLOR);
            return true;
        }();
    }

    void ForceLinkPureColorMaterialDefinition() {}
}
