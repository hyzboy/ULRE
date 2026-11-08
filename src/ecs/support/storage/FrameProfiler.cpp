#include<hgl/ecs/support/storage/FrameProfiler.h>

namespace hgl::ecs
{
    namespace
    {
        /// perf 通道判定走**函数本地 static**：全局 static 化会在别的 TU 的静态初始化期
        /// 读到尚未构造的通道掩码（静态初始化顺序无保证）。
        bool ChannelDefault()
        {
            static const bool s_from_channel = ULRE_DEBUG_ON(Perf);
            return s_from_channel;
        }

        bool & ForceState()
        {
            static bool s_forced = false;
            return s_forced;
        }

        bool & ForceValue()
        {
            static bool s_value = false;
            return s_value;
        }

        uint64_t * UsTable()
        {
            static uint64_t s_us[kFrameProfileSlotCount] = {};
            return s_us;
        }

        uint64_t * CountTable()
        {
            static uint64_t s_count[kFrameProfileCountCount] = {};
            return s_count;
        }

        uint64_t & Samples()
        {
            static uint64_t s_samples = 0;
            return s_samples;
        }

        uint64_t & Dropped()
        {
            static uint64_t s_dropped = 0;
            return s_dropped;
        }

        const char * const kSlotNames[kFrameProfileSlotCount] =
        {
            "ScenePrePass",
            "AcquireSwapchain",
            "PreBeginFrame",
            "RenderCoreBegin",
            "TransformSubmit",
            "CameraPublish",
            "CollectPhase",
            "SyncWorldAddresses",
            "BatchPhase",
            "BufferCommit",
            "BufferUpload",
            "FrameSync",
            "BeginRenderPass",
            "DrawRecord",
            "EndFrame",
            "SubmitPresent",
            "CollectPreScan",
            "CollectMainLoop",
            "CollectPublish",
            "BatchBuildMaterial",
            "BatchGPUCullGuard",
            "BatchFinalize",
            "AppTick",
            "SampleReadback",
        };

        const char * const kCountNames[kFrameProfileCountCount] =
        {
            "CollectPrimitives",
            "CollectItems",
            "Batches",
            "BucketGroups",
            "CollectCalls",
        };
    }//namespace

    const char * GetFrameProfileSlotName(FrameProfileSlot slot)
    {
        const uint32_t index = static_cast<uint32_t>(slot);

        return (index < kFrameProfileSlotCount) ? kSlotNames[index] : "?";
    }

    const char * GetFrameProfileCountName(FrameProfileCount slot)
    {
        const uint32_t index = static_cast<uint32_t>(slot);

        return (index < kFrameProfileCountCount) ? kCountNames[index] : "?";
    }

    void FrameProfiler::SetEnabled(bool on)
    {
        ForceState() = true;
        ForceValue() = on;
    }

    bool FrameProfiler::IsEnabled()
    {
        return ForceState() ? ForceValue() : ChannelDefault();
    }

    void FrameProfiler::Add(FrameProfileSlot slot, uint64_t us)
    {
        if (!IsEnabled())
            return;

        const uint32_t index = static_cast<uint32_t>(slot);

        if (index < kFrameProfileSlotCount)
            UsTable()[index] += us;
    }

    void FrameProfiler::Count(FrameProfileCount slot, uint64_t n)
    {
        if (!IsEnabled())
            return;

        const uint32_t index = static_cast<uint32_t>(slot);

        if (index < kFrameProfileCountCount)
            CountTable()[index] += n;
    }

    uint64_t FrameProfiler::GetUs(FrameProfileSlot slot)
    {
        const uint32_t index = static_cast<uint32_t>(slot);

        return (index < kFrameProfileSlotCount) ? UsTable()[index] : 0;
    }

    uint64_t FrameProfiler::GetCount(FrameProfileCount slot)
    {
        const uint32_t index = static_cast<uint32_t>(slot);

        return (index < kFrameProfileCountCount) ? CountTable()[index] : 0;
    }

    void FrameProfiler::Reset()
    {
        uint64_t *us = UsTable();
        uint64_t *count = CountTable();

        for (uint32_t i = 0; i < kFrameProfileSlotCount; ++i)
            us[i] = 0;

        for (uint32_t i = 0; i < kFrameProfileCountCount; ++i)
            count[i] = 0;

        Samples() = 0;
        Dropped() = 0;
    }

    uint64_t FrameProfiler::SumUs()
    {
        const uint64_t *us = UsTable();

        // 只累加**互不重叠的顶层槽**：`ScenePrePass..SubmitPresent` 连续 16 项，
        // 加 `AppTick` 与 `SampleReadback`。收集/合批的**子槽**是顶层槽的拆分，
        // 参与相加会把同一段时间算两遍 ⇒ 显式排除。
        uint64_t sum = 0;

        for (uint32_t i = static_cast<uint32_t>(FrameProfileSlot::ScenePrePass);
                        i <= static_cast<uint32_t>(FrameProfileSlot::SubmitPresent); ++i)
            sum += us[i];

        sum += us[static_cast<uint32_t>(FrameProfileSlot::AppTick)];
        sum += us[static_cast<uint32_t>(FrameProfileSlot::SampleReadback)];

        return sum;
    }

    uint64_t FrameProfiler::SampleCount()
    {
        return Samples();
    }

    void FrameProfiler::CountSample()
    {
        if (IsEnabled())
            ++Samples();
    }

    void FrameProfiler::DropSample()
    {
        if (IsEnabled())
            ++Dropped();
    }

    uint64_t FrameProfiler::DroppedSamples()
    {
        return Dropped();
    }
}//namespace hgl::ecs
