#pragma once

#include <hgl/ecs/components/PrimitiveComponent.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/mtl/MaterialKey.h>
#include <hgl/mtl/MaterialRecipe.h>

#include <cstdint>
#include <functional>
#include <memory>

namespace hgl::ecs::internal
{
    struct ResolveTask
    {
        std::shared_ptr<PrimitiveComponent> comp;
        graph::MaterialResolveRequest *slot = nullptr;
        graph::Geometry *geometry = nullptr;
        const graph::mtl::MaterialRecipe *recipe = nullptr;
        graph::mtl::MaterialKey material_key{};
        uint64_t gvf_hash = 0;
        uint64_t instance_hash = 0;
    };

    struct PrototypeKey
    {
        graph::mtl::MaterialKey material_key{};
        uint64_t gvf_hash = 0;

        bool operator==(const PrototypeKey &o) const noexcept
        {
            return material_key == o.material_key && gvf_hash == o.gvf_hash;
        }
    };

    struct PrototypeKeyHash
    {
        size_t operator()(const PrototypeKey &k) const noexcept
        {
            const size_t h1 = static_cast<size_t>(k.material_key.Hash());
            const size_t h2 = static_cast<size_t>(k.gvf_hash + 0x9e3779b97f4a7c15ull);
            return h1 ^ (h2 + 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
        }
    };

    struct MIKey
    {
        uint64_t prototype_hash = 0;
        uint64_t instance_hash = 0;

        bool operator==(const MIKey &o) const noexcept
        {
            return prototype_hash == o.prototype_hash
                && instance_hash == o.instance_hash;
        }
    };

    struct MIKeyHash
    {
        size_t operator()(const MIKey &k) const noexcept
        {
            const size_t h1 = std::hash<uint64_t>{}(k.prototype_hash);
            const size_t h2 = std::hash<uint64_t>{}(k.instance_hash);
            return h1 ^ (h2 + 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
        }
    };

    uint64_t HashBytes(const void *data, uint32_t size);
    uint64_t HashGeometryVertexFormat(const graph::GeometryVertexFormat &gvf);
    uint64_t HashPrototypeKey(const PrototypeKey &k) noexcept;
}
