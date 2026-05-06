#pragma once

#include <hgl/mtl/StaticMaterialDef.h>
#include <hgl/mtl/SamplerSlot.h>

namespace hgl::graph::mtl
{

const SSBOSemanticSet &GetStandardBaseSSBOs() noexcept;

const SamplerSlot *GetStandardTextureSlots(uint32_t &slot_count) noexcept;

// Canonical Standard material definition shared by both recipe-key and
// runtime creation paths.
StaticMaterialDef BuildCanonicalStandardStaticDef(bool any_array) noexcept;

} // namespace hgl::graph::mtl
