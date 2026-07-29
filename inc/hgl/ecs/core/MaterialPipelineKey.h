#pragma once

#include <functional>
#include <cstdint>

namespace hgl
{
    namespace graph
    {
        class MaterialProgram;
        class Pipeline;
    }
}

namespace hgl::ecs
{
    /**
     * MaterialProgram/Pipeline index for batching.
     * All active batches are recipe runtime batches.
     */
    struct MaterialPipelineKey
    {
        hgl::graph::MaterialProgram* material;
        hgl::graph::Pipeline* pipeline;
        uint64_t materialization_spec_hash;        // ProgramSignature（不含 domain/offset）
        uint64_t materialization_domain_signature; // BindingSignature（含 domain/资源身份）

        MaterialPipelineKey(hgl::graph::MaterialProgram* m = nullptr,
                            hgl::graph::Pipeline* p = nullptr,
                            uint64_t program_signature = 0,
                            uint64_t binding_signature = 0)
            : material(m), pipeline(p), materialization_spec_hash(program_signature), materialization_domain_signature(binding_signature) {}

        bool operator<(const MaterialPipelineKey& other) const
        {
            if (material < other.material) return true;
            if (material > other.material) return false;
            if (pipeline < other.pipeline) return true;
            if (pipeline > other.pipeline) return false;
            if (materialization_spec_hash < other.materialization_spec_hash) return true;
            if (materialization_spec_hash > other.materialization_spec_hash) return false;
            return materialization_domain_signature < other.materialization_domain_signature;
        }

        bool operator==(const MaterialPipelineKey& other) const
        {
            return material == other.material
                && pipeline == other.pipeline
                && materialization_spec_hash == other.materialization_spec_hash
                && materialization_domain_signature == other.materialization_domain_signature;
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
            size_t h1 = std::hash<hgl::graph::MaterialProgram*>{}(key.material);
            size_t h2 = std::hash<hgl::graph::Pipeline*>{}(key.pipeline);
            size_t h3 = std::hash<uint64_t>{}(key.materialization_spec_hash);
            size_t h4 = std::hash<uint64_t>{}(key.materialization_domain_signature);
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
        }
    };
}//namespace std
