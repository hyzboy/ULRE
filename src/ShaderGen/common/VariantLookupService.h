#pragma once

#include <hgl/mtl/MaterialVariantRegistry.h>

namespace hgl::graph::mtl::routing
{

MaterialVariantKey CanonicalizeRegistryLookupKey(const MaterialVariantKey &key,
                                                 const RegistryLookupOptions &options);

} // namespace hgl::graph::mtl::routing
