#pragma once

#include <functional>
#include <cstdint>

namespace hgl
{
    namespace graph
    {
        class Material;
        class Pipeline;
    }
}

namespace hgl::ecs
{
    /**
     * Material/Pipeline index for batching
     * Similar to hgl::graph::PipelineMaterialIndex
     */
    struct MaterialPipelineKey
    {
        hgl::graph::Material* material;
        hgl::graph::Pipeline* pipeline;
        uint64_t materialization_spec_hash;

        MaterialPipelineKey(hgl::graph::Material* m = nullptr,
                            hgl::graph::Pipeline* p = nullptr,
                            uint64_t spec_hash = 0)
            : material(m), pipeline(p), materialization_spec_hash(spec_hash) {}

        bool operator<(const MaterialPipelineKey& other) const
        {
            if (material < other.material) return true;
            if (material > other.material) return false;
            if (pipeline < other.pipeline) return true;
            if (pipeline > other.pipeline) return false;
            return materialization_spec_hash < other.materialization_spec_hash;
        }

        bool operator==(const MaterialPipelineKey& other) const
        {
            return material == other.material
                && pipeline == other.pipeline
                && materialization_spec_hash == other.materialization_spec_hash;
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
            size_t h3 = std::hash<uint64_t>{}(key.materialization_spec_hash);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}//namespace std
