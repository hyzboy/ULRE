#include "VariantLookupService.h"
#include "VariantKeyOps.h"

#include <atomic>
#include <cstdio>

namespace hgl::graph::mtl::routing
{

namespace
{

#if defined(ULRE_SHADERGEN_VERBOSE)
constexpr bool kVariantRegistryVerbose = true;
#else
constexpr bool kVariantRegistryVerbose = false;
#endif

}

MaterialVariantKey CanonicalizeRegistryLookupKey(const MaterialVariantKey &key,
                                                 const RegistryLookupOptions &options)
{
    MaterialVariantKey canon = key;

    if (!options.match_effective_feature_mask && canon.effective_feature_mask != 0)
    {
        static std::atomic_bool s_warned_effective_mask_ignored{false};
        bool expected = false;
        if (s_warned_effective_mask_ignored.compare_exchange_strong(expected, true, std::memory_order_relaxed))
        {
            std::fprintf(stderr,
                "[VariantRegistry] warning: effective_feature_mask is ignored for variant routing by default; "
                "set RegistryLookupOptions::match_effective_feature_mask = true to include it in registry lookup.\n");
        }

        canon.effective_feature_mask = 0;
    }

    // Strip vertex-pulling provider bits for registry lookup.
    // The variant registry selects shader descriptor layouts and does NOT
    // distinguish vertex-stream sources.
    for (auto &p : canon.attribute_providers)
        p = AttributeProviderId::None;
    if (canon.position_provider == PositionProviderId::SSBO_PackedVec3)
        canon.position_provider = PositionProviderId::DirectVec3;

    // Standard Mesh3D descriptor selection is not split by sky model.
    if (canon.surface_type == SurfaceType::Standard
     && canon.geometry_mode == GeometryMode::Mesh3D)
    {
        canon.sky_ambient_model = SkyLightAmbientModel::Simple;
    }

    return canon;
}

bool ResolveVariantForKey(const MaterialVariantKey &request_key,
                          const VariantRegistry &registry,
                          VariantLookupResult &out,
                          const RegistryLookupOptions &options)
{
    out.request_key = request_key;
    out.lookup_key = CanonicalizeRegistryLookupKey(request_key, options);

    if (kVariantRegistryVerbose && !(out.lookup_key == request_key))
    {
        std::fprintf(stderr,
            "[VariantRegistry] canonicalized lookup request={%s} canonical={%s}\n",
            FormatVariantKeyForLog(request_key, true).c_str(),
            FormatVariantKeyForLog(out.lookup_key, true).c_str());
    }

    const MaterialVariantDesc *desc = registry.QueryVariant(out.lookup_key, options);

    out.resolved_key = out.lookup_key;
    out.variant_desc = desc;

    if (desc)
    {
        if (auto *s = GetGlobalVariantRegistryStatsSink())
            s->OnExactMatch(out.lookup_key, *desc);

        if (kVariantRegistryVerbose)
        {
            std::fprintf(stderr,
                "[VariantRegistry] exact-match variant=%s %s\n",
                desc->variant_name.empty() ? "<unnamed>" : desc->variant_name.c_str(),
                FormatVariantKeyForLog(out.lookup_key, true).c_str());
        }
        return true;
    }

    if (auto *s = GetGlobalVariantRegistryStatsSink())
        s->OnMiss(out.lookup_key);

    std::fprintf(stderr,
        "[VariantRegistry] miss request={%s}\n",
        FormatVariantKeyForLog(out.lookup_key, true).c_str());

    return desc != nullptr;
}

bool ResolveBuiltinVariantForKey(const MaterialVariantKey &request_key,
                                 VariantLookupResult &out,
                                 const RegistryLookupOptions &options)
{
    return ResolveVariantForKey(request_key,
                                GetBuiltinVariantRegistry(),
                                out,
                                options);
}

} // namespace hgl::graph::mtl::routing
