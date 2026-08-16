#include <hgl/mtl/MaterialDefinitionRegistry.h>

namespace hgl::graph::mtl{
void RegisterText2DMaterialDefinition()
{
        MaterialDefinition definition{};
        definition.definition_id = BUILTIN_MTL_DEF_TEXT;
        definition.definition_name = "Text2D";
        definition.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        definition.bootstrap_kind = MaterialDefinitionBootstrapKind::TextAlphaBlend;
        definition.ubo_requirements = {UBODescriptorSemantic::ViewportInfo};
        definition.data_slot_decls = {{"mtl", SSBOType::TransmissionSurface}};
        definition.vertex_node_config = Make2DNodeConfigOrtho(false);
        MaterialVertexVaryingConfig varying{};
        varying.emit_data_index_id = true;
        varying.emit_uv0 = true;
        SetMaterialFragmentSource(
            definition, "compositor/main_forward_surface.frag.glsl");
        definition.fragment_surface_module = "surface/material_surface.glsl";
        definition.fragment_material_source_module =
            "material/text_source.glsl";
        definition.sampler_names = {"Nearest"};
        definition.vertex_varying = varying;
        const GLSLCodeModuleSemanticRequirement vertex_requirements[] = {
            MakeMaterialVertexSemanticRequirement(VertexSemantic::Position),
            MakeMaterialVertexSemanticRequirement(VertexSemantic::TexCoord)
        };
        ConfigureMaterialVertexSemanticContract(
            definition, vertex_requirements, 2, MaterialVertexProviderPolicy::GeometryOnly);
        RegisterMaterialDefinition(definition);
}

}//namespace hgl::graph::mtl