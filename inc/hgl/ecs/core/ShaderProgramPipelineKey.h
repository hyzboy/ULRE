#pragma once

#include <functional>
#include <cstdint>

namespace hgl
{
    namespace graph
    {
        class ShaderProgram;
        class Pipeline;
    }
}

namespace hgl::ecs
{
    /**
     * ShaderProgram/Pipeline index for batching.
     * All active batches are recipe runtime batches.
     */
    struct ShaderProgramPipelineKey
    {
        hgl::graph::ShaderProgram* shader_program;
        hgl::graph::Pipeline* pipeline;
        uint64_t ssbo_binding_signature;

        ShaderProgramPipelineKey(hgl::graph::ShaderProgram* m = nullptr,
                            hgl::graph::Pipeline* p = nullptr,
                            uint64_t ssbo_signature = 0)
            : shader_program(m), pipeline(p), ssbo_binding_signature(ssbo_signature) {}

        bool operator<(const ShaderProgramPipelineKey& other) const
        {
            if (shader_program < other.shader_program) return true;
            if (shader_program > other.shader_program) return false;
            if (pipeline < other.pipeline) return true;
            if (pipeline > other.pipeline) return false;
            return ssbo_binding_signature < other.ssbo_binding_signature;
        }

        bool operator==(const ShaderProgramPipelineKey& other) const
        {
            return shader_program == other.shader_program
                && pipeline == other.pipeline
                && ssbo_binding_signature == other.ssbo_binding_signature;
        }
    };
}//namespace hgl::ecs

// Hash specialization for std::hash (required for unordered containers)
namespace std
{
    template<>
    struct hash<hgl::ecs::ShaderProgramPipelineKey>
    {
        size_t operator()(const hgl::ecs::ShaderProgramPipelineKey& key) const noexcept
        {
            size_t h1 = std::hash<hgl::graph::ShaderProgram*>{}(key.shader_program);
            size_t h2 = std::hash<hgl::graph::Pipeline*>{}(key.pipeline);
            size_t h3 = std::hash<uint64_t>{}(key.ssbo_binding_signature);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}//namespace std
