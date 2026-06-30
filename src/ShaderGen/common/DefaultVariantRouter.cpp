#include "DefaultVariantRouter.h"

namespace hgl::graph::mtl
{

GenericVariantPolicyResult BuildGenericVariantPolicy(const MaterialVariantKey &input_key,
                                                     const GenericVariantPolicyConfig &config)
{
    GenericVariantPolicyResult result;

    for (uint8 i = 0; i < static_cast<uint8>(SamplerSlot::RANGE_SIZE); ++i)
    {
        const SamplerSlot slot = static_cast<SamplerSlot>(i);
        result.resolved_modes[static_cast<size_t>(i)] = input_key.GetTextureSourceMode(slot);
    }

    const uint32 rule_count = config.slot_rule_count;

    if (config.any_array_checks_all_slots)
    {
        for (uint8 i = 0; i < static_cast<uint8>(SamplerSlot::RANGE_SIZE); ++i)
        {
            if (result.resolved_modes[static_cast<size_t>(i)] == TextureSourceMode::Array)
            {
                result.any_array = true;
                break;
            }
        }
    }
    else
    {
        for (uint32 i = 0; i < rule_count; ++i)
        {
            if (!config.slot_rules[i].participates_in_any_array)
                continue;

            const SamplerSlot slot = config.slot_rules[i].slot;
            if (result.resolved_modes[static_cast<size_t>(slot)] == TextureSourceMode::Array)
            {
                result.any_array = true;
                break;
            }
        }
    }

    result.route_key = input_key;
    result.route_key.surface_type = config.surface;

    {
        const TextureSourceMode route_mode = result.any_array
            ? TextureSourceMode::Array
            : TextureSourceMode::Simple;

        for (uint32 i = 0; i < rule_count; ++i)
        {
            if (!config.slot_rules[i].normalize_route_mode_to_any_array)
                continue;

            result.route_key.SetTextureSourceMode(config.slot_rules[i].slot, route_mode);
        }
    }

    for (uint32 i = 0; i < rule_count; ++i)
    {
        if (!config.slot_rules[i].force_route_has_texture)
            continue;

        // Ensure the feature bit is set. If no source mode has been assigned yet,
        // default to Simple so that source_bits and feature_bits stay consistent.
        const SamplerSlot fslot = config.slot_rules[i].slot;
        if (result.route_key.GetTextureSourceMode(fslot) == TextureSourceMode::None)
            result.route_key.SetTextureSourceMode(fslot, TextureSourceMode::Simple);
    }

    result.assemble_key = result.route_key;
    for (uint32 i = 0; i < rule_count; ++i)
    {
        if (!config.slot_rules[i].copy_resolved_mode_to_assemble)
            continue;

        const SamplerSlot slot = config.slot_rules[i].slot;
        result.assemble_key.SetTextureSourceMode(slot, result.resolved_modes[static_cast<size_t>(slot)]);
    }

    return result;
}

} // namespace hgl::graph::mtl
