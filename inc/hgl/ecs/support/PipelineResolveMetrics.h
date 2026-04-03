#pragma once

#include <atomic>
#include <cstdint>
#include <hgl/log/Log.h>

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

    inline void LogPipelineResolveCreated(const char* owner,
                                          const uint64_t vkcreate_delta,
                                          const PipelineResolveCounters& counters)
    {
        if (vkcreate_delta == 0)
            return;

        GLogInfo("[%s] Pipeline resolve created vk pipelines=%llu (attempts=%llu successes=%llu failures=%llu)",
                 owner,
                 static_cast<unsigned long long>(vkcreate_delta),
                 static_cast<unsigned long long>(counters.attempts.load()),
                 static_cast<unsigned long long>(counters.successes.load()),
                 static_cast<unsigned long long>(counters.failures.load()));
    }

    inline void LogPipelineResolveFrameSummary(const char* owner,
                                               const uint32_t frame_attempts,
                                               const uint32_t frame_successes,
                                               const uint32_t frame_failures,
                                               const PipelineResolveCounters& counters)
    {
        if (!frame_attempts && !frame_failures)
            return;

        const uint64_t success_total = counters.successes.load();
        const uint64_t failure_total = counters.failures.load();

        if (frame_failures > 0 || ShouldLogPow2(success_total + failure_total))
        {
            GLogDebug("[%s] Pipeline resolve summary: attempts=%u success=%u fail=%u totals(s=%llu,f=%llu)",
                      owner,
                      frame_attempts,
                      frame_successes,
                      frame_failures,
                      static_cast<unsigned long long>(success_total),
                      static_cast<unsigned long long>(failure_total));
        }
    }

    inline void LogPipelineHotpathCreateViolation(const char* owner,
                                                  const uint64_t vkcreate_delta,
                                                  PipelineHotpathCounters& counters)
    {
        if (vkcreate_delta == 0)
            return;

        const uint64_t violations = ++counters.violations;
        if (ShouldLogPow2(violations))
        {
            GLogWarning("[%s] Stage-4 violation: render hot path created %llu vk pipeline(s), total_violations=%llu",
                        owner,
                        static_cast<unsigned long long>(vkcreate_delta),
                        static_cast<unsigned long long>(violations));
        }
    }

}
