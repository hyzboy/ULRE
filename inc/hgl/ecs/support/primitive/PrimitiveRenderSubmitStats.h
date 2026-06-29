#pragma once

#include <cstdint>

namespace hgl
{
    namespace graph
    {
        class GraphicsPipeline;
        class ShaderMaterialProgram;
    }

    namespace ecs
    {
        class ECSContext;

        struct PrimitiveRenderSubmitFrameStats
        {
            uint64_t batches_seen = 0;
            uint64_t program_bind_calls = 0;
            uint64_t descriptor_bind_calls = 0;
            uint64_t program_switches = 0;
            uint64_t descriptor_switches = 0;
        };

        class PrimitiveRenderSubmitStats
        {
        private:
            const char *owner_tag = "[ECS::PrimitiveRenderSystem][R4]";
            bool periodic_log_enabled = true;
            uint64_t submit_frames = 0;
            graph::GraphicsPipeline *last_pipeline = nullptr;
            graph::ShaderMaterialProgram *last_program = nullptr;
            const void *last_binding_token = nullptr;
            PrimitiveRenderSubmitFrameStats frame_stats{};

        public:
            explicit PrimitiveRenderSubmitStats(const char *owner_tag_text = "[ECS::PrimitiveRenderSystem][R4]",
                                               bool enable_periodic_log = true)
                : owner_tag(owner_tag_text)
                , periodic_log_enabled(enable_periodic_log)
            {
            }

            void OnBatch(graph::GraphicsPipeline *pipeline,
                         graph::ShaderMaterialProgram *program,
                         const void *binding_token,
                         bool &out_skip_pipeline_bind,
                         bool &out_skip_descriptor_bind);

            const PrimitiveRenderSubmitFrameStats &GetFrameStats() const { return frame_stats; }

            void CommitAndMaybeLog(ECSContext *context);
        };
    }
}
