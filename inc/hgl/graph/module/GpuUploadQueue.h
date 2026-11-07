#pragma once

#include <hgl/vk/VK.h>
#include <hgl/type/ValueArray.h>
#include <hgl/type/UnorderedMap.h>
#include <hgl/type/OrderedSet.h>
#include <hgl/sync/Mutex.h>
#include <hgl/log/Log.h>
#include <hgl/graph/module/GpuUploadTask.h>

namespace hgl::graph
{
    class VulkanDevice;
    class DeviceQueue;
    class TextureCmdBuffer;
    class Semaphore;
    class StagingRingBuffer;

    /**
     * 通用 GPU 异步上传队列。
     * 不区分上传载荷是什么——只区分两种硬件 DMA 形态：
     *  - BufferToImage：纹理 / Cubemap / 纹理数组（vkCmdCopyBufferToImage）
     *  - BufferToBuffer：几何 VBO/IBO、SSBO 等任意原始字节块（vkCmdCopyBuffer）
     * 支持四级优先级（Immediate/High/Normal/Low）、可撤销（Cancel/Discard）、
     * Staging显存背压控制、双队列提交（专用 Transfer 队列 DMA / Graphics 队列 DMA）、
     * 零拷贝直通与环形 Staging 显存复用池。
     */
    class GpuUploadQueue
    {
        OBJECT_LOGGER

    private:
        VulkanDevice *      device_              = nullptr;

        DeviceQueue *       transfer_queue_      = nullptr;
        TextureCmdBuffer *  transfer_cmd_buf_    = nullptr;

        DeviceQueue *       graphics_queue_      = nullptr;
        TextureCmdBuffer *  graphics_cmd_buf_    = nullptr;

        StagingRingBuffer * transfer_ring_       = nullptr;
        StagingRingBuffer * graphics_ring_       = nullptr;

        VkDeviceSize        max_staging_bytes_   = 128ULL * 1024 * 1024; // 默认 128 MB 预算
        VkDeviceSize        current_staging_bytes_ = 0;

        uint64_t            next_task_id_        = 1;

        // 四级就绪队列
        ValueArray<GpuUploadTask *> queued_tasks_[static_cast<size_t>(UploadPriority::Count)];

        // 在飞任务列表
        ValueArray<GpuUploadTask *> in_flight_tasks_;

        // 已完成任务列表（等待提取进行 Bindless 更新）
        ValueArray<GpuUploadTask *> completed_tasks_;

        // 任务 ID 映射（用于快速查找和撤销）
        UnorderedMap<uint64_t, GpuUploadTask *> all_tasks_;

        // 已经完成并提取的历史任务集合
        OrderedSet<uint64_t> completed_task_ids_;

        // 跨队列同步 = **单条 upload lane**（timeline）：每次 transfer 派发 signal 一个递增值，
        // 主帧渲染提交前 wait 最新值 ⇒ 覆盖全部在飞 DMA 拷贝，且等待项恒为 1 个。
        // 旧方案（逐任务二进制信号量收集）有两处坏：① 批量完成任务的 sem 在批次执行完毕前仍
        //   “in use”，任务析构 delete → vkDestroySemaphore 踩 VUID-vkDestroySemaphore-semaphore-05149；
        // ② 收集数量可超主帧 waits[MAX_WAITS]，溢出丢弃 = 无等待裸用刚拷贝的纹理。
        Semaphore * upload_lane_       = nullptr;
        uint64_t    upload_lane_value_ = 0;      ///< 已提交的最新信号值（0 = 从未派发）

        Mutex         mutex_;

    private:
        void EnsureResources();
        bool DispatchTask(GpuUploadTask *task);
        bool DispatchGpuTransferDma(GpuUploadTask *task);
        bool DispatchGpuGraphicsDma(GpuUploadTask *task);
        bool DispatchBufferToBuffer(GpuUploadTask *task);

    public:
        GpuUploadQueue(VulkanDevice *device);
        ~GpuUploadQueue();

        /** 设置 Staging 显存上限预算（字节） */
        void SetMaxStagingMemory(VkDeviceSize max_bytes) { max_staging_bytes_ = max_bytes; }
        VkDeviceSize GetMaxStagingMemory() const { return max_staging_bytes_; }
        VkDeviceSize GetCurrentStagingMemory() const { return current_staging_bytes_; }

        /** 入队一个新上传任务，返回唯一任务ID */
        uint64_t Enqueue(GpuUploadTask *task);

        /**
         * 便捷入口：Staging -> DeviceBuffer 单段异步拷贝（BufferToBuffer）。
         * staging 需含 VK_BUFFER_USAGE_TRANSFER_SRC_BIT 且已写入数据；
         * target 需含 VK_BUFFER_USAGE_TRANSFER_DST_BIT（通常为 Device-Local 几何/SSBO 缓冲）。
         * owns_staging=true 时任务完成后销毁 staging（与零拷贝纹理所用约定一致）；
         * 共享/常驻 Staging 传 false，由调用方管理生命周期。
         */
        uint64_t EnqueueBufferCopy(DeviceBuffer *staging, VkDeviceSize src_offset,
                                   BufferOwner *target, VkDeviceSize dst_offset, VkDeviceSize size,
                                   UploadPriority priority = UploadPriority::Normal,
                                   void (*on_complete)(GpuUploadTask *, void *) = nullptr,
                                   void *user_data = nullptr,
                                   bool owns_staging = true);

        /** 便捷入口：Staging -> Device-Local 缓冲（BufferOwner）多段区域拷贝（一次提交、一次完成回调） */
        uint64_t EnqueueBufferCopyRegions(DeviceBuffer *staging, BufferOwner *target,
                                   const ValueArray<BufferCopyRegion> &regions,
                                   UploadPriority priority = UploadPriority::Normal,
                                   void (*on_complete)(GpuUploadTask *, void *) = nullptr,
                                   void *user_data = nullptr,
                                   bool owns_staging = true);

        /**
         * 撤销指定任务。
         * 若任务在 Queued 状态：直接从队列移除并标记 Cancelled；
         * 若任务在 InFlight 状态：标记为 Discarded，GPU 完成后静默销毁，不更新描述符。
         */
        bool CancelUpload(uint64_t task_id);

        /** 获取任务当前状态 */
        UploadTaskState GetTaskState(uint64_t task_id);

        /** 查找任务对象指针（注意多线程生命周期） */
        GpuUploadTask *FindTask(uint64_t task_id);

        /** 同步等待某个任务完成（通常用于 Immediate 任务） */
        void WaitTask(uint64_t task_id);

        /** 帧轮询调度：回收已完成任务、依据优先级和背压调度新任务 */
        void Update();

        /** 上传车道（nullptr = 未初始化）；主帧渲染提交前对 GetUploadLaneValue() 发起等待 */
        Semaphore *GetUploadLane() const { return upload_lane_; }

        /** 已提交的最新信号值（0 = 从未派发 ⇒ 无需等待） */
        uint64_t GetUploadLaneValue() const { return upload_lane_value_; }

        /** 提取当前已完成的任务列表（移交给外部进行 Bindless 更新） */
        void ExtractCompletedTasks(ValueArray<GpuUploadTask *> &out_completed);
    };
}
