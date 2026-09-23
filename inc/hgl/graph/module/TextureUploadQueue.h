#pragma once

#include <hgl/vk/VK.h>
#include <hgl/type/ValueArray.h>
#include <hgl/type/UnorderedMap.h>
#include <hgl/type/OrderedSet.h>
#include <hgl/thread/ThreadMutex.h>
#include <hgl/log/Log.h>
#include <hgl/graph/module/TextureUploadTask.h>

namespace hgl::graph
{
    class VulkanDevice;
    class DeviceQueue;
    class TextureCmdBuffer;
    class Semaphore;

    /**
     * 统一纹理上传队列管理器。
     * 支持四级优先级（Immediate/High/Normal/Low）、可撤销（Cancel/Discard）、
     * Staging显存背压控制、双路径提交（HostImageCopy直写 / GPU DMA传输）。
     */
    class TextureUploadQueue
    {
        OBJECT_LOGGER

    private:
        VulkanDevice *      device_              = nullptr;

        DeviceQueue *       transfer_queue_      = nullptr;
        TextureCmdBuffer *  transfer_cmd_buf_    = nullptr;

        DeviceQueue *       graphics_queue_      = nullptr;
        TextureCmdBuffer *  graphics_cmd_buf_    = nullptr;

        VkDeviceSize        max_staging_bytes_   = 128ULL * 1024 * 1024; // 默认 128 MB 预算
        VkDeviceSize        current_staging_bytes_ = 0;

        uint64_t            next_task_id_        = 1;

        // 四级就绪队列
        ValueArray<TextureUploadTask *> queued_tasks_[static_cast<size_t>(UploadPriority::Count)];

        // 在飞任务列表
        ValueArray<TextureUploadTask *> in_flight_tasks_;

        // 已完成任务列表（等待提取进行 Bindless 更新）
        ValueArray<TextureUploadTask *> completed_tasks_;

        // 任务 ID 映射（用于快速查找和撤销）
        UnorderedMap<uint64_t, TextureUploadTask *> all_tasks_;

        // 已经完成并提取的历史任务集合
        OrderedSet<uint64_t> completed_task_ids_;

        // 收集本帧由 Transfer 队列发出、需 Graphics 队列在渲染前等待的信号量
        ValueArray<VkSemaphore> pending_wait_semaphores_;

        ThreadMutex         mutex_;

    private:
        void EnsureResources();
        bool DispatchTask(TextureUploadTask *task);
        bool DispatchHostImageCopy(TextureUploadTask *task);
        bool DispatchGpuTransferDma(TextureUploadTask *task);
        bool DispatchGpuGraphicsDma(TextureUploadTask *task);

    public:
        TextureUploadQueue(VulkanDevice *device);
        ~TextureUploadQueue();

        /** 设置 Staging 显存上限预算（字节） */
        void SetMaxStagingMemory(VkDeviceSize max_bytes) { max_staging_bytes_ = max_bytes; }
        VkDeviceSize GetMaxStagingMemory() const { return max_staging_bytes_; }
        VkDeviceSize GetCurrentStagingMemory() const { return current_staging_bytes_; }

        /** 入队一个新上传任务，返回唯一任务ID */
        uint64_t Enqueue(TextureUploadTask *task);

        /**
         * 撤销指定任务。
         * 若任务在 Queued 状态：直接从队列移除并标记 Cancelled；
         * 若任务在 InFlight 状态：标记为 Discarded，GPU 完成后静默销毁，不更新描述符。
         */
        bool CancelUpload(uint64_t task_id);

        /** 获取任务当前状态 */
        UploadTaskState GetTaskState(uint64_t task_id);

        /** 查找任务对象指针（注意多线程生命周期） */
        TextureUploadTask *FindTask(uint64_t task_id);

        /** 同步等待某个任务完成（通常用于 Immediate 任务） */
        void WaitTask(uint64_t task_id);

        /** 帧轮询调度：回收已完成任务、依据优先级和背压调度新任务 */
        void Update();

        /** 获取需 Graphics 队列等待的 Transfer 完成信号量合集 */
        const ValueArray<VkSemaphore> &GetPendingWaitSemaphores() const { return pending_wait_semaphores_; }

        /** 清空已接入等待链的信号量列表 */
        void ClearPendingWaitSemaphores() { pending_wait_semaphores_.Clear(); }

        /** 提取当前已完成的任务列表（移交给外部进行 Bindless 更新） */
        void ExtractCompletedTasks(ValueArray<TextureUploadTask *> &out_completed);
    };
}
