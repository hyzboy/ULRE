#pragma once

#include <hgl/mtl/MaterialLibrary.h>

namespace hgl::graph::mtl::routing
{

bool IsSemanticMaterialPreset(MaterialPreset preset) noexcept;

MaterialPreset ResolvePresetForLOD(MaterialPreset preset,
                                   MaterialLOD lod) noexcept;

const char *GetPresetName(MaterialPreset preset) noexcept;

MaterialVariantKey BuildRouteKey(MaterialPreset preset,
                                 uint32 extra_attrib_bits,
                                 const RuntimeKeyOverrides &ov) noexcept;

} // namespace hgl::graph::mtl::routing
