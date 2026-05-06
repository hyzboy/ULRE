#pragma once

#include <hgl/mtl/MaterialVariantRegistry.h>

namespace hgl::graph::mtl::routing
{

struct VariantLookupResult
{
    MaterialVariantKey request_key{};
    MaterialVariantKey lookup_key{};
    MaterialVariantKey resolved_key{};
    const MaterialVariantDesc *variant_desc = nullptr;
};

MaterialVariantKey CanonicalizeRegistryLookupKey(const MaterialVariantKey &key,
                                                 const RegistryLookupOptions &options);

bool ResolveVariantForKey(const MaterialVariantKey &request_key,
                          const VariantRegistry &registry,
                          VariantLookupResult &out,
                          const RegistryLookupOptions &options = {});

bool ResolveBuiltinVariantForKey(const MaterialVariantKey &request_key,
                                 VariantLookupResult &out,
                                 const RegistryLookupOptions &options = {});

} // namespace hgl::graph::mtl::routing
