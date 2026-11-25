#pragma once

#include<hgl/vk/VKQueue.h>

namespace hgl::ecs
{
    /**
     * 帧提交等待列表（世界私有）。
     *
     * 两条独立车道：
     * - **帧车道累积**：本帧已提交的各离屏 RT 车道值（`AddFrameLaneWait`），供主帧提交
     *   统一 await；主帧提交即本帧收尾，累积归零。
     * - **下一次提交的额外等待**：由 `SetSubmitWaits` 显式设置，或由帧车道累积
     *   （`SetSubmitWaitsFromFrameLanes`）提升而来；`TakeSubmitWaits` 取走即清空。
     *
     * 纯状态机，不持有世界/设备指针：生命周期与析构顺序由世界成员布局决定。
     */
    class FrameLaneWaitSet
    {
    public:
        static constexpr uint32_t kMaxLaneWaits = 16;

    private:
        graph::SemaphoreSubmit frame_lane_waits[kMaxLaneWaits];
        uint32_t frame_lane_wait_count = 0;

        graph::SemaphoreSubmit submit_waits[kMaxLaneWaits];
        uint32_t submit_wait_count = 0;

    public:
        /// 设置下一次提交的额外等待列表（空信号量条目被跳过；超出上限的尾部被丢弃）
        void SetSubmitWaits(const graph::SemaphoreSubmit *waits, uint32_t count);

        /// 登记一条本帧已提交的离屏 RT 车道（lane 为空或 value==0 时忽略）
        void AddFrameLaneWait(graph::Semaphore *lane, uint64_t value);

        /// 把帧车道累积提升为下一次提交的额外等待
        void SetSubmitWaitsFromFrameLanes();

        /// 取走并清空下一次提交的等待列表；`is_main_frame` 为真时同时清空帧车道累积
        const graph::SemaphoreSubmit *TakeSubmitWaits(uint32_t &count, bool is_main_frame);
    };
}