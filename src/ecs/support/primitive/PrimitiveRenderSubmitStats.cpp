#include <hgl/ecs/support/primitive/PrimitiveRenderSubmitStats.h>

#include <hgl/ecs/core/Context.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    void PrimitiveRenderSubmitStats::OnBatch(graph::GraphicsPipeline *pipeline,
                                             graph::ShaderMaterialProgram *program,
                                             const void *binding_token,
                                             bool &out_skip_pipeline_bind,
                                             bool &out_skip_descriptor_bind)
    {
        ++frame_stats.batches_seen;

        if (program && program != last_program)
        {
            ++frame_stats.program_switches;
            last_program = program;
        }

        const bool binding_changed = (binding_token != last_binding_token);
        if (binding_token && binding_changed)
            ++frame_stats.descriptor_switches;

        out_skip_pipeline_bind = (pipeline == last_pipeline);
        last_pipeline = pipeline;
        if (!out_skip_pipeline_bind)
            ++frame_stats.program_bind_calls;

        out_skip_descriptor_bind = !binding_changed
                                && (binding_token != nullptr)
                                && (frame_stats.batches_seen > 1);
        if (!out_skip_descriptor_bind)
            ++frame_stats.descriptor_bind_calls;

        last_binding_token = binding_token;
    }

    void PrimitiveRenderSubmitStats::CommitAndMaybeLog(ECSContext *context)
    {
        if (!context)
            return;

        auto &diag = context->GetMaterialResolveDiagnostics();
        diag.RecordR4RenderSubmitStats(frame_stats.batches_seen,
                                       frame_stats.program_bind_calls,
                                       frame_stats.descriptor_bind_calls,
                                       frame_stats.program_switches,
                                       frame_stats.descriptor_switches);

        if (!periodic_log_enabled)
            return;

        ++submit_frames;
        if ((submit_frames & (submit_frames - 1)) != 0)
            return;

        const auto ToPercent = [](const uint64_t saved, const uint64_t baseline) -> double
        {
            if (baseline == 0)
                return 0.0;

            return (100.0 * static_cast<double>(saved)) / static_cast<double>(baseline);
        };

        MaterialResolveR4Stats total_stats{};
        diag.GetR4Stats(total_stats);

        const uint64_t frame_program_saved = frame_stats.batches_seen > frame_stats.program_bind_calls
                                           ? (frame_stats.batches_seen - frame_stats.program_bind_calls)
                                           : 0;
        const uint64_t frame_descriptor_saved = frame_stats.batches_seen > frame_stats.descriptor_bind_calls
                                              ? (frame_stats.batches_seen - frame_stats.descriptor_bind_calls)
                                              : 0;

        GLogInfo("%s submit frame=%llu batches=%llu program_bind=%llu descriptor_bind=%llu program_saved=%llu(%.2f%%) descriptor_saved=%llu(%.2f%%) program_switch=%llu descriptor_switch=%llu",
             owner_tag,
             static_cast<unsigned long long>(submit_frames),
                 static_cast<unsigned long long>(frame_stats.batches_seen),
                 static_cast<unsigned long long>(frame_stats.program_bind_calls),
                 static_cast<unsigned long long>(frame_stats.descriptor_bind_calls),
                 static_cast<unsigned long long>(frame_program_saved),
                 ToPercent(frame_program_saved, frame_stats.batches_seen),
                 static_cast<unsigned long long>(frame_descriptor_saved),
                 ToPercent(frame_descriptor_saved, frame_stats.batches_seen),
                 static_cast<unsigned long long>(frame_stats.program_switches),
                 static_cast<unsigned long long>(frame_stats.descriptor_switches));

        GLogInfo("%s cumulative submits=%llu batches=%llu program_bind=%llu descriptor_bind=%llu program_saved=%llu(%.2f%%) descriptor_saved=%llu(%.2f%%)",
             owner_tag,
                 static_cast<unsigned long long>(total_stats.submit_calls_total),
                 static_cast<unsigned long long>(total_stats.batches_seen_total),
                 static_cast<unsigned long long>(total_stats.program_bind_calls_total),
                 static_cast<unsigned long long>(total_stats.descriptor_bind_calls_total),
                 static_cast<unsigned long long>(total_stats.program_bind_skipped_total),
                 ToPercent(total_stats.program_bind_skipped_total, total_stats.batches_seen_total),
                 static_cast<unsigned long long>(total_stats.descriptor_bind_skipped_total),
                 ToPercent(total_stats.descriptor_bind_skipped_total, total_stats.batches_seen_total));
    }
}
