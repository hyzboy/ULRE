#pragma once

#include <functional>
#include <hgl/graph/MRDHandle.h>

namespace hgl
{
    namespace graph
    {
        class MaterialTemplate;
        class GraphicsPipeline;
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
     * MaterialTemplate/GraphicsPipeline/Domain/Queue index for batching.
     * P10: domain field migrated from raw MaterialResourceDomain* to MRDHandle
     * so batch identity is stable across domain pointer invalidation.
     * domain_handle == {} → default (no domain, backward-compatible)
     */
    struct MaterialPipelineKey
    {
        hgl::graph::MaterialTemplate*   material      = nullptr;
        hgl::graph::GraphicsPipeline*   pipeline      = nullptr;
        hgl::graph::MRDHandle           domain_handle = {};     ///< P10: was MaterialResourceDomain*
        RenderQueue                     queue         = RenderQueue::Opaque;

        MaterialPipelineKey(hgl::graph::MaterialTemplate* m = nullptr,
                            hgl::graph::GraphicsPipeline* p = nullptr,
                            hgl::graph::MRDHandle         dh = {},
                            RenderQueue                   q  = RenderQueue::Opaque)
            : material(m), pipeline(p), domain_handle(dh), queue(q) {}

        bool operator<(const MaterialPipelineKey& other) const
        {
            if (material < other.material) return true;
            if (material > other.material) return false;
            if (pipeline < other.pipeline) return true;
            if (pipeline > other.pipeline) return false;
            if (domain_handle.id < other.domain_handle.id) return true;
            if (domain_handle.id > other.domain_handle.id) return false;
            if (domain_handle.generation < other.domain_handle.generation) return true;
            if (domain_handle.generation > other.domain_handle.generation) return false;
            return queue < other.queue;
        }

        bool operator==(const MaterialPipelineKey& other) const
        {
            return material      == other.material
                && pipeline      == other.pipeline
                && domain_handle == other.domain_handle
                && queue         == other.queue;
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
            size_t h1 = std::hash<hgl::graph::MaterialTemplate*>{}(key.material);
            size_t h2 = std::hash<hgl::graph::GraphicsPipeline*>{}(key.pipeline);
            size_t h3 = std::hash<uint32_t>{}(key.domain_handle.id)
                      ^ (std::hash<uint32_t>{}(key.domain_handle.generation) << 16);
            size_t h4 = std::hash<int>{}(static_cast<int>(key.queue));
            // Combine hashes
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
        }
    };
}//namespace std
