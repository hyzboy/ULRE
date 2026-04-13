#include "StandardDescriptorBuilder.h"

#include "../common/MFSkyLight.h"

#include <hgl/mtl/SamplerSlot.h>

namespace hgl::graph::mtl
{

void BuildStandardDescriptorState(
    const Material3DCreateConfig *cfg,
    const SamplerSlot *tex_slots,
    const TextureSourceMode *tex_slot_modes,
    const uint32_t tex_slot_count,
    const bool policy_any_array,
    const SSBOSemanticSet &base_ssbos,
    Material3DCreateConfig &cfg_with_mi,
    SkyLightAmbientModel &ambient,
    LightingModel &lighting,
    SSBOSemanticSet &dynamic_ssbos,
    StaticTextureSamplerDescriptors &dynamic_samplers,
    std::vector<const char *> &unused_resources,
    bool &any_array)
{
    cfg_with_mi = cfg ? *cfg : Material3DCreateConfig();
    cfg_with_mi.material_instance = true;

    ambient = cfg_with_mi.sky_ambient_model;
    lighting = cfg_with_mi.lighting_model;

    any_array = policy_any_array;

    dynamic_ssbos = base_ssbos;
    dynamic_samplers.clear();

    for (uint32_t i = 0; i < tex_slot_count; ++i)
    {
        const SamplerType sampler_type = (tex_slot_modes[i] == TextureSourceMode::Array)
            ? SamplerType::Sampler2DArray
            : SamplerType::Sampler2D;
        AddTextureSampler(dynamic_samplers, tex_slots[i], sampler_type);
    }

    if (any_array)
        AddSSBODescriptor(dynamic_ssbos, SSBODescriptorSemantic::MaterialInstanceTextureID);

    unused_resources.clear();
    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(ambient),
        dynamic_samplers,
        unused_resources);
}

StaticMaterialDef BuildStandardDynamicDef(
    const StaticMaterialDef &def_template,
    SSBOSemanticSet &dynamic_ssbos,
    StaticTextureSamplerDescriptors &dynamic_samplers,
    InstanceDataLayout mi_layout,
    const bool any_array,
    const VertexAttributeSpec *vertex_spec_override,
    const uint32_t vertex_spec_override_count)
{
    StaticMaterialDef dynamic_def = def_template;
    dynamic_def.ssbo_descriptors = &dynamic_ssbos;
    dynamic_def.texture_samplers = &dynamic_samplers;
    dynamic_def.mi_instance_layout = mi_layout;
    dynamic_def.name = any_array ? "StandardTextureArray_v1" : "Standard_v1";
    if (vertex_spec_override && vertex_spec_override_count > 0)
    {
        dynamic_def.vertex_attribute_specs = vertex_spec_override;
        dynamic_def.vertex_attribute_spec_count = vertex_spec_override_count;
    }
    return dynamic_def;
}

} // namespace hgl::graph::mtl
