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
     * A6-2b/b3：ssbo_binding_signature 字段已删——BDA 后同 shader 即同批，
     * key 只按 shader+pipeline 区分。
     */
    struct ShaderProgramPipelineKey
    {
        hgl::graph::ShaderProgram* shader_program;
        hgl::graph::Pipeline* pipeline;

        ShaderProgramPipelineKey(hgl::graph::ShaderProgram* m = nullptr,
                            hgl::graph::Pipeline* p = nullptr)
            : shader_program(m), pipeline(p) {}

        bool operator<(const ShaderProgramPipelineKey& other) const
        {
            if (shader_program < other.shader_program) return true;
            if (shader_program > other.shader_program) return false;
            if (pipeline < other.pipeline) return true;
            if (pipeline > other.pipeline) return false;
            return false;
        }

        bool operator==(const ShaderProgramPipelineKey& other) const
        {
            return shader_program == other.shader_program
                && pipeline == other.pipeline;
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
            return h1 ^ (h2 << 1);
        }
    };
}//namespace std
