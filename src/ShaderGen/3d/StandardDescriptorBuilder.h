#pragma once

#include <hgl/mtl/FixedMaterialDef.h>
#include <hgl/mtl/Material3DCreateConfig.h>

#include <vector>

namespace hgl::graph::mtl
{

void BuildStandardDescriptorState(
    const Material3DCreateConfig *cfg,
    TextureSourceMode resolved_base,
    TextureSourceMode resolved_normal,
    const SamplerSlot *tex_slots,
    uint32_t tex_slot_count,
    const FixedSSBODescriptors &base_ssbos,
    Material3DCreateConfig &cfg_with_mi,
    SkyLightAmbientModel &ambient,
    LightingModel &lighting,
    FixedSSBODescriptors &dynamic_ssbos,
    FixedTextureSamplerDescriptors &dynamic_samplers,
    std::vector<const char *> &unused_resources,
    bool &any_array);

FixedMaterialDef BuildStandardDynamicDef(
    const FixedMaterialDef &def_template,
    FixedSSBODescriptors &dynamic_ssbos,
    FixedTextureSamplerDescriptors &dynamic_samplers,
    const char *mi_codes,
    uint32_t mi_bytes,
    bool any_array);

} // namespace hgl::graph::mtl
