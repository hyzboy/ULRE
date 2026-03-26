#pragma once

#include <functional>

namespace hgl
{
    namespace graph
    {
        class Material;
        class Pipeline;
        class ResourceDomain;    // Phase 4
    }
}

namespace hgl::ecs
{
    enum class RenderQueue
    {
        Opaque = 0,
        Masked,
        Transparent,
        Overlay
    };

    /**
     * Material/Pipeline/Domain/Queue index for batching
     * Phase 4: ResourceDomain* added so items from different domains
     * do not get incorrectly merged into the same draw batch.
     * domain == nullptr → default (backward-compatible with all existing code)
     */
    struct MaterialPipelineKey
    {
        hgl::graph::Material*       material = nullptr;
        hgl::graph::Pipeline*       pipeline = nullptr;
        hgl::graph::ResourceDomain* domain   = nullptr;   ///< Phase 4: nullptr = default domain
        RenderQueue                 queue    = RenderQueue::Opaque;

        MaterialPipelineKey(hgl::graph::Material*       m = nullptr,
                            hgl::graph::Pipeline*       p = nullptr,
                            hgl::graph::ResourceDomain* d = nullptr,
                            RenderQueue                 q = RenderQueue::Opaque)
            : material(m), pipeline(p), domain(d), queue(q) {}

        bool operator<(const MaterialPipelineKey& other) const
        {
            if (material < other.material) return true;
            if (material > other.material) return false;
            if (pipeline < other.pipeline) return true;
            if (pipeline > other.pipeline) return false;
            if (domain < other.domain) return true;
            if (domain > other.domain) return false;
            return queue < other.queue;
        }

        bool operator==(const MaterialPipelineKey& other) const
        {
            return material == other.material
                && pipeline == other.pipeline
                && domain   == other.domain
                && queue    == other.queue;
        }
    };
}//namespace hgl::ecs

// Hash specialization for std::hash (required for unordered containers)
namespace std
{
    template<>
    struct hash<hgl::ecs::MaterialPipelineKey>
    {
        size_t operator()(const hgl::ecs::MaterialPipelineKey& key) const noexcept
        {
            size_t h1 = std::hash<hgl::graph::Material*>{}(key.material);
            size_t h2 = std::hash<hgl::graph::Pipeline*>{}(key.pipeline);
            size_t h3 = std::hash<hgl::graph::ResourceDomain*>{}(key.domain);
            size_t h4 = std::hash<int>{}(static_cast<int>(key.queue));
            // Combine hashes
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
        }
    };
}//namespace std
