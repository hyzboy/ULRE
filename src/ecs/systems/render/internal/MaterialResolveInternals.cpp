#include "MaterialResolveInternals.h"

namespace hgl::ecs::internal
{
    uint64_t HashBytes(const void *data, const uint32_t size)
    {
        if (!data || size == 0)
            return 1469598103934665603ull;

        const auto *bytes = static_cast<const uint8_t *>(data);
        uint64_t h = 1469598103934665603ull;

        for (uint32_t i = 0; i < size; ++i)
        {
            h ^= static_cast<uint64_t>(bytes[i]);
            h *= 1099511628211ull;
        }

        return h;
    }

    uint64_t HashGeometryVertexFormat(const graph::GeometryVertexFormat &gvf)
    {
        uint64_t h = 1469598103934665603ull;

        const auto mix = [&h](const uint64_t v)
        {
            h ^= v;
            h *= 1099511628211ull;
        };

        mix(static_cast<uint64_t>(gvf.GetActiveCount()));

        for (int i = 0; i < static_cast<int>(graph::VertexAttrib::RANGE_SIZE); ++i)
        {
            const auto attrib = static_cast<graph::VertexAttrib>(i);
            const auto *slot = gvf.GetSlot(attrib);
            if (!slot)
                continue;

            mix(static_cast<uint64_t>(i + 1));
            mix(static_cast<uint64_t>(slot->format));
            mix(static_cast<uint64_t>(slot->stride));
            mix(static_cast<uint64_t>(static_cast<uint32_t>(slot->binding + 1)));
        }

        return h;
    }

    uint64_t HashPrototypeKey(const PrototypeKey &k) noexcept
    {
        const uint64_t mk = k.material_key.Hash();
        return (mk * 1099511628211ull) ^ (k.gvf_hash + 0x9e3779b97f4a7c15ull + (mk << 6) + (mk >> 2));
    }
}
