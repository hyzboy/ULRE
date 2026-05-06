#pragma once

#include <hgl/mtl/MaterialVariantKey.h>

#include <string>

namespace hgl::graph::mtl
{

struct MaterialCreateConfig;

namespace routing
{

// Format a variant key for diagnostics.
// include_extended_fields=true adds blend/pass/effective-mask fields used by registry logs.
std::string FormatVariantKeyForLog(const MaterialVariantKey &key,
                                   bool include_extended_fields = false);

// Apply cfg-derived runtime overrides to an existing variant key.
void ApplyCreateConfigOverrides(MaterialVariantKey &key,
                                const MaterialCreateConfig *cfg);

} // namespace routing

} // namespace hgl::graph::mtl
