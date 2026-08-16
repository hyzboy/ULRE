#include <hgl/mtl/MaterialDefinitionRegistry.h>

namespace hgl::graph::mtl
{
    void RegisterPureColorMaterialDefinition()
    {
            MaterialDefinition definition{};
            definition.definition_id = BUILTIN_MTL_DEF_PURE_COLOR;
            definition.definition_name = "builtin/pure_color";
            definition.source_kind = MaterialDefinitionSourceKind::BuiltIn;
            definition.usage_tag = MaterialDefinitionUsageTag::Fallback;
            definition.bootstrap_kind = MaterialDefinitionBootstrapKind::PureColor;
            definition.data_slot_decls = {{"mtl", SSBOType::EmissiveSurface}};
            definition.ubo_requirements = {
                UBODescriptorSemantic::ViewportInfo,
                UBODescriptorSemantic::CameraInfo
            };
            definition.vertex_node_config = MakeDefault3DNodeConfig();
            SetMaterialFragmentSource(
                definition, "compositor/main_forward_surface.frag.glsl");
            definition.fragment_surface_module =
                "surface/material_surface.glsl";
            definition.fragment_material_source_module =
                "material/unlit_source.glsl";
            definition.vertex_varying.emit_data_index_id = true;

            const GLSLCodeModuleSemanticRequirement requirements[] = {
                MakeMaterialVertexSemanticRequirement(VertexSemantic::Position)
            };
            ConfigureMaterialVertexSemanticContract(
                definition, requirements, 1, MaterialVertexProviderPolicy::GeometryOnly);
            RegisterMaterialDefinition(definition);
            RegisterMaterialDefinitionAlias(
                   BUILTIN_MTL_DEF_MISSING_MATERIAL,
                   BUILTIN_MTL_DEF_PURE_COLOR);
    }
}
