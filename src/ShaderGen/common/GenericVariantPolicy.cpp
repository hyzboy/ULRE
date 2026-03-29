#include "GenericVariantPolicy.h"

namespace hgl::graph::mtl
{

namespace
{

inline SamplerSlot GetRuleSlot(const GenericVariantPolicyConfig &config, const uint32 index)
{
    return config.slot_rules[index].slot;
}

inline bool RuleParticipatesInAnyArray(const GenericVariantPolicyConfig &config, const uint32 index)
{
    return config.slot_rules[index].participates_in_any_array;
}

inline bool RuleNormalizeRouteModeToAnyArray(const GenericVariantPolicyConfig &config, const uint32 index)
{
    return config.slot_rules[index].normalize_route_mode_to_any_array;
}

inline bool RuleForceRouteHasTexture(const GenericVariantPolicyConfig &config, const uint32 index)
{
    return config.slot_rules[index].force_route_has_texture;
}

inline bool RuleCopyResolvedModeToAssemble(const GenericVariantPolicyConfig &config, const uint32 index)
{
    return config.slot_rules[index].copy_resolved_mode_to_assemble;
}

inline uint32 GetEffectiveRuleCount(const GenericVariantPolicyConfig &config)
{
    if (config.slot_rules && config.slot_rule_count > 0)
        return config.slot_rule_count;

    return config.controlled_slot_count;
}

inline SamplerSlot GetEffectiveSlot(const GenericVariantPolicyConfig &config, const uint32 index)
{
    if (config.slot_rules && config.slot_rule_count > 0)
        return GetRuleSlot(config, index);

    return config.controlled_slots[index];
}

inline bool EffectiveParticipatesInAnyArray(const GenericVariantPolicyConfig &config, const uint32 index)
{
    if (config.slot_rules && config.slot_rule_count > 0)
        return RuleParticipatesInAnyArray(config, index);

    return true;
}

inline bool EffectiveNormalizeRouteModeToAnyArray(const GenericVariantPolicyConfig &config, const uint32 index)
{
    if (config.slot_rules && config.slot_rule_count > 0)
        return RuleNormalizeRouteModeToAnyArray(config, index);

    return config.normalize_controlled_slots_to_any_array;
}

inline bool EffectiveForceRouteHasTexture(const GenericVariantPolicyConfig &config, const uint32 index)
{
    if (config.slot_rules && config.slot_rule_count > 0)
        return RuleForceRouteHasTexture(config, index);

    return config.force_has_texture_for_controlled_slots;
}

inline bool EffectiveCopyResolvedModeToAssemble(const GenericVariantPolicyConfig &config, const uint32 index)
{
    if (config.slot_rules && config.slot_rule_count > 0)
        return RuleCopyResolvedModeToAssemble(config, index);

    return true;
}

} // namespace

GenericVariantPolicyResult BuildGenericVariantPolicy(const MaterialVariantKey &input_key,
                                                     const GenericVariantPolicyConfig &config)
{
    GenericVariantPolicyResult result;

    for (uint8 i = 0; i < static_cast<uint8>(SamplerSlot::RANGE_SIZE); ++i)
    {
        const SamplerSlot slot = static_cast<SamplerSlot>(i);
        result.resolved_modes[static_cast<size_t>(i)] = input_key.GetTextureSourceMode(slot);
    }

    const uint32 rule_count = GetEffectiveRuleCount(config);

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
            if (!EffectiveParticipatesInAnyArray(config, i))
                continue;

            const SamplerSlot slot = GetEffectiveSlot(config, i);
            if (result.resolved_modes[static_cast<size_t>(slot)] == TextureSourceMode::Array)
            {
                result.any_array = true;
                break;
            }
        }
    }

    result.route_key = input_key;
    result.route_key.surface_type = config.surface;

    if (config.normalize_controlled_slots_to_any_array || (config.slot_rules && config.slot_rule_count > 0))
    {
        const TextureSourceMode route_mode = result.any_array
            ? TextureSourceMode::Array
            : TextureSourceMode::Simple;

        for (uint32 i = 0; i < rule_count; ++i)
        {
            if (!EffectiveNormalizeRouteModeToAnyArray(config, i))
                continue;

            const SamplerSlot slot = GetEffectiveSlot(config, i);
            result.route_key.SetTextureSourceMode(slot, route_mode);
        }
    }

    if (config.force_has_texture_for_controlled_slots || (config.slot_rules && config.slot_rule_count > 0))
    {
        for (uint32 i = 0; i < rule_count; ++i)
        {
            if (!EffectiveForceRouteHasTexture(config, i))
                continue;

            const SamplerSlot slot = GetEffectiveSlot(config, i);
            result.route_key.SetHasTexture(slot);
        }
    }

    result.assemble_key = result.route_key;
    for (uint32 i = 0; i < rule_count; ++i)
    {
        if (!EffectiveCopyResolvedModeToAssemble(config, i))
            continue;

        const SamplerSlot slot = GetEffectiveSlot(config, i);
        result.assemble_key.SetTextureSourceMode(slot, result.resolved_modes[static_cast<size_t>(slot)]);
    }

    return result;
}

} // namespace hgl::graph::mtl
