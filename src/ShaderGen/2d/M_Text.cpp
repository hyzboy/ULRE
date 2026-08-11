#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/mtl/SamplerName.h>

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredText2DBmi = []() -> bool
    {
        MaterialDefinition definition{};
        definition.definition_id = BUILTIN_MTL_DEF_TEXT;
        definition.definition_name = "Text2D";
        definition.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        definition.usage_tag = MaterialDefinitionUsageTag::Text;
        definition.bootstrap_kind = MaterialDefinitionBootstrapKind::TextAlphaBlend;
        definition.ubo_requirements = {UBODescriptorSemantic::ViewportInfo};
        definition.texture_slot_decls = {{TextureSlot::BaseColor, GLSLSamplerType::Sampler2D, true, SamplerName::Text}};
        definition.data_slot_decls = {{"mtl", SSBOType::TransmissionSurface}};
        definition.vertex_node_config = Make2DNodeConfigOrtho(false);
        MaterialVertexVaryingConfig varying{};
        varying.emit_data_index_id = true;
        varying.emit_texture_layer_id = true;
        varying.emit_uv0 = true;
        SetMaterialFragmentSource(
            definition, "compositor/main_forward_surface.frag.glsl");
        definition.fragment_surface_module = "surface/material_surface.glsl";
        definition.fragment_material_source_module =
            "material/text_source.glsl";
        definition.vertex_varying = varying;
        const GLSLCodeModuleSemanticRequirement vertex_requirements[] = {
            MakeMaterialVertexSemanticRequirement(VertexSemantic::Position),
            MakeMaterialVertexSemanticRequirement(VertexSemantic::TexCoord)
        };
        ConfigureMaterialVertexSemanticContract(
            definition, vertex_requirements, 2, MaterialVertexProviderPolicy::GeometryOnly);
        RegisterMaterialDefinition(definition);
        RegisterMaterialDefinitionAlias("Text2D", BUILTIN_MTL_DEF_TEXT);

        return true;
    }();
}

void ForceLinkText2DMaterialDefinition() {}

}//namespace hgl::graph::mtl