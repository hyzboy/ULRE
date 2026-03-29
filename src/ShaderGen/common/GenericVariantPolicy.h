#pragma once

#include <hgl/mtl/new/MaterialVariantKey.h>
#include <hgl/mtl/SamplerName.h>

#include <array>

namespace hgl::graph::mtl
{

struct GenericVariantPolicySlotRule
{
    SamplerSlot slot = SamplerSlot::BaseColor;

    bool participates_in_any_array = true;
    bool normalize_route_mode_to_any_array = true;
    bool force_route_has_texture = true;
    bool copy_resolved_mode_to_assemble = true;
};

struct GenericVariantPolicyConfig
{
    SurfaceType surface = SurfaceType::Standard;

    const GenericVariantPolicySlotRule *slot_rules = nullptr;
    uint32 slot_rule_count = 0;

    bool any_array_checks_all_slots = false;
};

struct GenericVariantPolicyResult
{
    std::array<TextureSourceMode, SamplerSlotCount> resolved_modes{};

    bool any_array = false;

    MaterialVariantKey route_key;
    MaterialVariantKey assemble_key;
};

GenericVariantPolicyResult BuildGenericVariantPolicy(const MaterialVariantKey &input_key,
                                                     const GenericVariantPolicyConfig &config);

} // namespace hgl::graph::mtl
