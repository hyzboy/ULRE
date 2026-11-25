#pragma once

#include<hgl/vk/VK.h>
#include<cstdint>

namespace hgl::graph
{
    class Semaphore;

    /// 上传车道的一次等待项（timeline 信号量 + 已提交的最新信号值）
    struct UploadLaneSignal
    {
        Semaphore *semaphore = nullptr;
        uint64_t   value     = 0;

        /// 是否构成一个真实等待项（无车道 / 从未派发 ⇒ 假）
        bool IsValid() const { return semaphore != nullptr && value != 0; }
    };

    /**
     * 上传车道的**提供方契约**（下层需要、上层实现）。
     *
     * 为什么需要它：交换链提交必须把「本帧所有在飞 DMA 拷贝」串进等待列表，而车道
     * 归上层的纹理管理器所有。直接持有 `TextureManager*` 会让 Vulkan 层反向 include
     * 上层模块（也就把 swapchain RT 钉死在一个具体实现上）。
     *
     * 本接口只暴露一个纯值：车道信号量与已提交值。下层不需要知道上传队列、任务表
     * 或任何上层类型。
     */
    class UploadLaneProvider
    {
    public:
        virtual ~UploadLaneProvider() = default;

        /// 当前上传车道的等待项（未就绪 ⇒ `IsValid()` 为假）
        virtual UploadLaneSignal GetUploadLaneSignal() const = 0;
    };
}//namespace hgl::graph