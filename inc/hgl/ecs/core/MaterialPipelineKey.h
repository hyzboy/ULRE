#pragma once

#include <functional>

namespace hgl
{
    namespace graph
    {
        class ShaderMaterialProgram;
        class GraphicsPipeline;
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
     * ShaderMaterialProgram/GraphicsPipeline/Domain/Queue index for batching
     * Phase 4: ResourceDomain* added so items from different domains
     * do not get incorrectly merged into the same draw batch.
     * domain == nullptr → default (backward-compatible with all existing code)
     */
    struct MaterialPipelineKey
    {
        // Legacy storage field kept for compatibility with existing serialization,
        // hashing and map key behavior. Runtime reads should use GetProgram().
        hgl::graph::ShaderMaterialProgram*       material = nullptr;
        hgl::graph::GraphicsPipeline*       pipeline = nullptr;
        hgl::graph::ResourceDomain* domain   = nullptr;   ///< Phase 4: nullptr = default domain
        RenderQueue                 queue    = RenderQueue::Opaque;

        MaterialPipelineKey(hgl::graph::ShaderMaterialProgram*       m = nullptr,
                            hgl::graph::GraphicsPipeline*       p = nullptr,
                            hgl::graph::ResourceDomain* d = nullptr,
                            RenderQueue                 q = RenderQueue::Opaque)
            : material(m), pipeline(p), domain(d), queue(q) {}

        hgl::graph::ShaderMaterialProgram* GetProgram() const { return material; }
        void SetProgram(hgl::graph::ShaderMaterialProgram* p) { material = p; }

        bool operator<(const MaterialPipelineKey& other) const
        {
            const auto *this_program = GetProgram();
            const auto *other_program = other.GetProgram();
            if (this_program < other_program) return true;
            if (this_program > other_program) return false;
            if (pipeline < other.pipeline) return true;
            if (pipeline > other.pipeline) return false;
            if (domain < other.domain) return true;
            if (domain > other.domain) return false;
            return queue < other.queue;
        }

        bool operator==(const MaterialPipelineKey& other) const
        {
            return GetProgram() == other.GetProgram()
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
            size_t h1 = std::hash<hgl::graph::ShaderMaterialProgram*>{}(key.GetProgram());
            size_t h2 = std::hash<hgl::graph::GraphicsPipeline*>{}(key.pipeline);
            size_t h3 = std::hash<hgl::graph::ResourceDomain*>{}(key.domain);
            size_t h4 = std::hash<int>{}(static_cast<int>(key.queue));
            // Combine hashes
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
        }
    };
}//namespace std
