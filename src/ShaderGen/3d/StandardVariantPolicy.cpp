#include "StandardVariantPolicy.h"

#include "../common/GenericVariantPolicy.h"

#include <hgl/mtl/SamplerName.h>

namespace hgl::graph::mtl
{

StandardVariantPolicyResult BuildStandardVariantPolicy(const MaterialVariantKey &input_key)
{
    constexpr SamplerSlot kStandardControlledSlots[] = {
        SamplerSlot::BaseColor,
        SamplerSlot::Normal,
    };

    GenericVariantPolicyConfig config;
    config.surface = SurfaceType::Standard;
    config.controlled_slots = kStandardControlledSlots;
    config.controlled_slot_count = uint32(sizeof(kStandardControlledSlots) / sizeof(kStandardControlledSlots[0]));
    config.normalize_controlled_slots_to_any_array = true;
    config.force_has_texture_for_controlled_slots = true;
    config.any_array_checks_all_slots = false;

    const GenericVariantPolicyResult generic = BuildGenericVariantPolicy(input_key, config);

    StandardVariantPolicyResult result;

    result.resolved_base = generic.resolved_modes[static_cast<size_t>(SamplerSlot::BaseColor)];
    result.resolved_normal = generic.resolved_modes[static_cast<size_t>(SamplerSlot::Normal)];

    result.base_is_array = (result.resolved_base == TextureSourceMode::Array);
    result.normal_is_array = (result.resolved_normal == TextureSourceMode::Array);
    result.any_array = generic.any_array;

    result.route_key = generic.route_key;
    result.assemble_key = generic.assemble_key;

    return result;
}

} // namespace hgl::graph::mtl
