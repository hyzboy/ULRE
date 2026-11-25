#include <hgl/ecs/core/FrameLaneWaitSet.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    void FrameLaneWaitSet::SetSubmitWaits(const graph::SemaphoreSubmit *waits, const uint32_t count)
    {
        submit_wait_count = 0;

        for (uint32_t i = 0; i < count && submit_wait_count < kMaxLaneWaits; i++)
            if (waits[i].semaphore)
                submit_waits[submit_wait_count++] = waits[i];
    }

    void FrameLaneWaitSet::AddFrameLaneWait(graph::Semaphore *lane, const uint64_t value)
    {
        if (!lane || value == 0)
            return;

        if (frame_lane_wait_count >= kMaxLaneWaits)
        {
            GLogWarning("[FrameLaneWaitSet] 车道等待列表已满(%u)，忽略 lane=%p value=%llu",
                        kMaxLaneWaits, static_cast<void *>(lane),
                        static_cast<unsigned long long>(value));
            return;
        }

        frame_lane_waits[frame_lane_wait_count++] = graph::SemaphoreSubmit::Wait(lane, value);
    }

    void FrameLaneWaitSet::SetSubmitWaitsFromFrameLanes()
    {
        SetSubmitWaits(frame_lane_waits, frame_lane_wait_count);
    }

    const graph::SemaphoreSubmit *FrameLaneWaitSet::TakeSubmitWaits(uint32_t &count, const bool is_main_frame)
    {
        count = submit_wait_count;
        submit_wait_count = 0;

        // 主帧提交即本帧收尾：清空累积，下一帧重新收集
        if (is_main_frame)
            frame_lane_wait_count = 0;

        return count > 0 ? submit_waits : nullptr;
    }
}