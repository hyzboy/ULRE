#include "GenericVariantPolicy.h"

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
        for (uint32 i = 0; i < config.controlled_slot_count; ++i)
        {
            const SamplerSlot slot = config.controlled_slots[i];
            if (result.resolved_modes[static_cast<size_t>(slot)] == TextureSourceMode::Array)
            {
                result.any_array = true;
                break;
            }
        }
    }

    result.route_key = input_key;
    result.route_key.surface_type = config.surface;

    if (config.normalize_controlled_slots_to_any_array)
    {
        const TextureSourceMode route_mode = result.any_array
            ? TextureSourceMode::Array
            : TextureSourceMode::Simple;

        for (uint32 i = 0; i < config.controlled_slot_count; ++i)
        {
            const SamplerSlot slot = config.controlled_slots[i];
            result.route_key.SetTextureSourceMode(slot, route_mode);
        }
    }

    if (config.force_has_texture_for_controlled_slots)
    {
        for (uint32 i = 0; i < config.controlled_slot_count; ++i)
        {
            const SamplerSlot slot = config.controlled_slots[i];
            result.route_key.SetHasTexture(slot);
        }
    }

    result.assemble_key = result.route_key;
    for (uint32 i = 0; i < config.controlled_slot_count; ++i)
    {
        const SamplerSlot slot = config.controlled_slots[i];
        result.assemble_key.SetTextureSourceMode(slot, result.resolved_modes[static_cast<size_t>(slot)]);
    }

    return result;
}

} // namespace hgl::graph::mtl
