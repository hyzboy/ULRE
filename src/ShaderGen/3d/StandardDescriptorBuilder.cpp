#include "StandardDescriptorBuilder.h"

#include "../common/MFSkyLight.h"

#include <hgl/mtl/SamplerName.h>

namespace hgl::graph::mtl
{

void BuildStandardDescriptorState(
    const Material3DCreateConfig *cfg,
    const TextureSourceMode resolved_base,
    const TextureSourceMode resolved_normal,
    const SamplerSlot *tex_slots,
    const uint32_t tex_slot_count,
    const FixedSSBODescriptors &base_ssbos,
    Material3DCreateConfig &cfg_with_mi,
    SkyLightAmbientModel &ambient,
    LightingModel &lighting,
    FixedSSBODescriptors &dynamic_ssbos,
    FixedTextureSamplerDescriptors &dynamic_samplers,
    std::vector<const char *> &unused_resources,
    bool &any_array)
{
    cfg_with_mi = cfg ? *cfg : Material3DCreateConfig();
    cfg_with_mi.material_instance = true;

    ambient = cfg_with_mi.sky_ambient_model;
    lighting = cfg_with_mi.lighting_model;

    const bool base_is_array = resolved_base == TextureSourceMode::Array;
    const bool normal_is_array = resolved_normal == TextureSourceMode::Array;
    any_array = base_is_array || normal_is_array;

    dynamic_ssbos = base_ssbos;
    dynamic_samplers.clear();

    const TextureSourceMode tex_slot_modes[] = {
        resolved_base,
        resolved_normal,
    };

    for (uint32_t i = 0; i < tex_slot_count; ++i)
    {
        const SamplerType sampler_type = (tex_slot_modes[i] == TextureSourceMode::Array)
            ? SamplerType::Sampler2DArray
            : SamplerType::Sampler2D;
        AddFixedTextureSampler(dynamic_samplers, tex_slots[i], sampler_type);
    }

    if (any_array)
        AddFixedSSBODescriptor(dynamic_ssbos, SSBODescriptorSemantic::MaterialInstanceTextureID);

    unused_resources.clear();
    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(ambient),
        dynamic_samplers,
        unused_resources);
}

FixedMaterialDef BuildStandardDynamicDef(
    const FixedMaterialDef &def_template,
    FixedSSBODescriptors &dynamic_ssbos,
    FixedTextureSamplerDescriptors &dynamic_samplers,
    const char *mi_codes,
    const uint32_t mi_bytes,
    const bool any_array)
{
    FixedMaterialDef dynamic_def = def_template;
    dynamic_def.ssbo_descriptors = &dynamic_ssbos;
    dynamic_def.texture_samplers = &dynamic_samplers;
    dynamic_def.mi_glsl_codes = mi_codes;
    dynamic_def.mi_struct_bytes = mi_bytes;
    dynamic_def.name = any_array ? "StandardTextureArray_v1" : "Standard_v1";
    return dynamic_def;
}

} // namespace hgl::graph::mtl
