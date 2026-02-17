#pragma once

#include <functional>

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

        MaterialPipelineKey(hgl::graph::Material* m = nullptr, hgl::graph::Pipeline* p = nullptr)
            : material(m), pipeline(p) {}

        bool operator<(const MaterialPipelineKey& other) const
        {
            if (material < other.material) return true;
            if (material > other.material) return false;
            return pipeline < other.pipeline;
        }

        bool operator==(const MaterialPipelineKey& other) const
        {
            return material == other.material && pipeline == other.pipeline;
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
            // Combine hashes using XOR and bit shift
            return h1 ^ (h2 << 1);
        }
    };
}//namespace std
