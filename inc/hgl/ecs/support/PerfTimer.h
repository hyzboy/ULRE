#pragma once

#include<cstddef>
#include<cstdint>
#include<cstdlib>
#include<hgl/log/Log.h>
#include<hgl/time/Time.h>

namespace hgl::ecs
{
    /**
     * PBR_PERF=1 计时代（临时插桩）：每 240 帧打印一次均值（稳态口径，启动杂项不进窗口）。
     *
     * `CullPerfTimer`（`GPUCullSystem.cpp`）与 `PerfTimer`（`PrimitiveBatchPipeline.cpp`）此前是
     * **两份逐字同构**的实现（`on = getenv("PBR_PERF") != nullptr` + `total_us` / `frames` +
     * `frames % 240` 窗口 + 归零），差别只有：名字在构造时给还是 `Add` 时给、以及是否追加一个
     * 可选诊断值（`gpu_dispatch` 的泄漏判据）⇒ 收成这一份。
     *
     * 计时用引擎单调 uptime（`hgl/time/Time.h`，禁 `std::chrono`）；未开启时 `Add` 直接返回。
     */
    struct PerfTimer
    {
        const bool  on;                 ///< PBR_PERF 是否开启（调用侧据此跳过取时刻）
        const char *name;               ///< 诊断标签
        uint64_t    total_us = 0;
        uint32_t    frames   = 0;

        explicit PerfTimer(const char *name_) : on(getenv("PBR_PERF") != nullptr), name(name_) {}

        bool Active() const { return on; }

        /// 累计一次。
        /// `probe_name` / `probe_value` 可选：非空时把 `probe_name=value` 追加在同一行
        /// （如 Vk 对象跟踪计数 = 泄漏判据）。取值成本归调用方（本函数不做任何额外采样）。
        void Add(uint64_t us, const char *probe_name = nullptr, size_t probe_value = 0)
        {
            if (!on)
                return;

            total_us += us;
            ++frames;

            if (frames % 240 != 0)
                return;

            if (probe_name)
            {
                GLogInfo("[PBR-PERF] %s: frames=%u avg=%.1f us/frame (total=%.1f ms) %s=%zu",
                         name, frames, double(total_us) / frames, double(total_us) / 1000.0,
                         probe_name, probe_value);
            }
            else
            {
                GLogInfo("[PBR-PERF] %s: frames=%u avg=%.1f us/frame (total=%.1f ms)",
                         name, frames, double(total_us) / frames, double(total_us) / 1000.0);
            }

            total_us = 0;
            frames   = 0;
        }
    };

    /// 计时用引擎时间源（单调 uptime）
    inline uint64_t DeltaUs(uint64_t t0_us)
    {
        return GetUptimeUs() - t0_us;
    }

    /// RAII 段（多点 return 的函数用它，出口统一累计）
    struct PerfSpan
    {
        PerfTimer  *timer;
        bool        on;
        uint64_t    t0;

        explicit PerfSpan(PerfTimer &t) : timer(&t), on(t.Active()), t0(on ? GetUptimeUs() : 0) {}

        ~PerfSpan()
        {
            if (on)
                timer->Add(DeltaUs(t0));
        }
    };
}//namespace ecs
