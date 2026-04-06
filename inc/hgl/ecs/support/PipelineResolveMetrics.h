#pragma once

#include <atomic>
#include <cstdint>

namespace hgl::ecs
{
    struct PipelineResolveCounters
    {
        ::std::atomic<uint64_t> attempts{0};
        ::std::atomic<uint64_t> successes{0};
        ::std::atomic<uint64_t> failures{0};
    };

    struct PipelineHotpathCounters
    {
        ::std::atomic<uint64_t> violations{0};
    };

    struct PipelineBatchPhaseTracker
    {
        uint64_t prev_batch_end_vkcreate = 0;
    };

    inline bool ShouldLogPow2(const uint64_t v)
    {
        return v != 0 && ((v & (v - 1)) == 0);
    }

    inline void RecordPipelineResolveAttempt(PipelineResolveCounters& counters)
    {
        ++counters.attempts;
    }

    inline void RecordPipelineResolveSuccess(PipelineResolveCounters& counters)
    {
        ++counters.successes;
    }

    inline uint64_t RecordPipelineResolveFailure(PipelineResolveCounters& counters)
    {
        return ++counters.failures;
    }

    inline bool ShouldLogPipelineResolveCreated(const uint64_t vkcreate_delta)
    {
        return vkcreate_delta > 0;
    }

    inline bool ShouldLogPipelineResolveFrameSummary(const uint32_t frame_attempts,
                                                     const uint32_t frame_failures,
                                                     const PipelineResolveCounters& counters,
                                                     uint64_t& success_total,
                                                     uint64_t& failure_total)
    {
        if (!frame_attempts && !frame_failures)
            return false;

        success_total = counters.successes.load();
        failure_total = counters.failures.load();
        return frame_failures > 0 || ShouldLogPow2(success_total + failure_total);
    }

    inline uint64_t RecordPipelineHotpathViolationAndGetLogCount(const uint64_t vkcreate_delta,
                                                                 PipelineHotpathCounters& counters)
    {
        if (vkcreate_delta == 0)
            return 0;

        const uint64_t violations = ++counters.violations;
        return ShouldLogPow2(violations) ? violations : 0;
    }

    inline uint64_t ComputeOutsideBatchPipelineCreation(const uint64_t vkcreate_at_start,
                                                        const PipelineBatchPhaseTracker& tracker)
    {
        if (tracker.prev_batch_end_vkcreate == 0)
            return 0;

        return vkcreate_at_start - tracker.prev_batch_end_vkcreate;
    }

    inline void EndPipelineBatchPhase(PipelineBatchPhaseTracker& tracker,
                                      const uint64_t vkcreate_at_end)
    {
        tracker.prev_batch_end_vkcreate = vkcreate_at_end;
    }

    inline bool ShouldLogPipelineResolveFrameSummaryWithSkips(const uint32_t frame_attempts,
                                                              const uint32_t frame_failures,
                                                              const uint32_t frame_skips,
                                                              const PipelineResolveCounters& counters,
                                                              const uint64_t total_skips,
                                                              uint64_t& success_total,
                                                              uint64_t& failure_total)
    {
        if (!frame_attempts && !frame_failures && !frame_skips)
            return false;

        success_total = counters.successes.load();
        failure_total = counters.failures.load();
        return frame_failures > 0 || ShouldLogPow2(success_total + failure_total + total_skips);
    }

}
