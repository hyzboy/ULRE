#pragma once

#include <hgl/mtl/new/MaterialVariantKey.h>
#include <hgl/mtl/SamplerName.h>

#include <array>

namespace hgl::graph::mtl
{

struct GenericVariantPolicyConfig
{
    SurfaceType surface = SurfaceType::Standard;

    const SamplerSlot *controlled_slots = nullptr;
    uint32 controlled_slot_count = 0;

    bool normalize_controlled_slots_to_any_array = true;
    bool force_has_texture_for_controlled_slots = true;
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
