#pragma once

#include <hgl/mtl/StaticMaterialDef.h>
#include <hgl/mtl/ShaderDataSchema.h>
#include <hgl/mtl/Material3DCreateConfig.h>

#include <vector>

namespace hgl::graph::mtl
{

void BuildStandardDescriptorState(
    const Material3DCreateConfig *cfg,
    const SamplerSlot *tex_slots,
    const TextureSourceMode *tex_slot_modes,
    uint32_t tex_slot_count,
    bool policy_any_array,
    const SSBOSemanticSet &base_ssbos,
    Material3DCreateConfig &cfg_with_mi,
    SkyLightAmbientModel &ambient,
    LightingModel &lighting,
    SSBOSemanticSet &dynamic_ssbos,
    StaticTextureSamplerDescriptors &dynamic_samplers,
    std::vector<const char *> &unused_resources,
    bool &any_array);

StaticMaterialDef BuildStandardDynamicDef(
    const StaticMaterialDef &def_template,
    SSBOSemanticSet &dynamic_ssbos,
    StaticTextureSamplerDescriptors &dynamic_samplers,
    ShaderDataSchema schema,
    bool any_array);

} // namespace hgl::graph::mtl
