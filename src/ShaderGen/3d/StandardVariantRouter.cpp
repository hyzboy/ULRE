#include "StandardVariantRouter.h"

#include "../common/DefaultVariantRouter.h"

#include <hgl/mtl/SamplerSlot.h>

namespace hgl::graph::mtl
{

StandardVariantPolicyResult BuildStandardVariantPolicy(const MaterialVariantKey &input_key)
{
    // Keep Standard schema-fixed: these rules model BaseColor+Normal only.
    // Additional texture semantics should be introduced as a separate material type.
    constexpr GenericVariantPolicySlotRule kStandardSlotRules[] = {
        {
            SamplerSlot::BaseColor,
            true,
            true,
            true,
            true,
        },
        {
            SamplerSlot::Normal,
            true,
            true,
            true,
            true,
        },
    };

    GenericVariantPolicyConfig config;
    config.surface = MaterialSurfaceClass::Standard;
    config.slot_rules = kStandardSlotRules;
    config.slot_rule_count = uint32(sizeof(kStandardSlotRules) / sizeof(kStandardSlotRules[0]));
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
