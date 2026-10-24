#pragma once

#include <hgl/mtl/PipelineConfig.h>
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
     *
     * ## 为什么还要带 `pipeline_config`（EDS 1/2/3）
     *
     * 管线对象**只由 Shader Stages + FBO 附件格式**决定，渲染状态全部走
     * `VK_DYNAMIC_STATE_*`（见 `inc/hgl/vk/pipeline/VKPipeline.h`）。
     * ⇒ 同一个 `Pipeline*` 会被**多种渲染形态**复用（如双面 vs 单面、不透明 vs 混合）。
     *
     * 而一个批只下发**一次**动态状态、发**一条** indirect multi-draw ⇒ 形态不同的图元
     * 绝不能进同一批，否则后者会套用前者的 cull/blend/depth 状态（历史 bug）。
     * 因此键必须含 `pipeline_config`：**同 shader + 同管线 + 同渲染形态** 才同批。
     * `overlay` 也在配置里（覆盖层 pass 分流读的就是 `key.pipeline_config.overlay`）。
     */
    struct ShaderProgramPipelineKey
    {
        hgl::graph::ShaderProgram* shader_program;
        hgl::graph::Pipeline* pipeline;
        hgl::graph::mtl::MaterialPipelineConfig pipeline_config;

        ShaderProgramPipelineKey(hgl::graph::ShaderProgram* m = nullptr,
                            hgl::graph::Pipeline* p = nullptr,
                            const hgl::graph::mtl::MaterialPipelineConfig& cfg = {})
            : shader_program(m), pipeline(p), pipeline_config(cfg) {}

        bool operator<(const ShaderProgramPipelineKey& other) const
        {
            if (shader_program < other.shader_program) return true;
            if (shader_program > other.shader_program) return false;
            if (pipeline < other.pipeline) return true;
            if (pipeline > other.pipeline) return false;
            return hgl::graph::mtl::LessMaterialPipelineConfig(pipeline_config,
                                                              other.pipeline_config);
        }

        bool operator==(const ShaderProgramPipelineKey& other) const
        {
            return shader_program == other.shader_program
                && pipeline == other.pipeline
                && pipeline_config == other.pipeline_config;
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
            size_t h3 = size_t(hgl::graph::mtl::HashMaterialPipelineConfig(key.pipeline_config));
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}//namespace std
