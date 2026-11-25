#pragma once

#include <hgl/vk/VK.h>
#include <hgl/type/ValueArray.h>
#include <hgl/type/UnorderedMap.h>
#include <hgl/type/OrderedSet.h>
#include <hgl/sync/Mutex.h>
#include <hgl/log/Log.h>
#include <hgl/graph/module/GpuUploadTask.h>
#include <hgl/graph/module/UploadTaskScheduler.h>

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

        /// 任务调度与状态机（就绪/在飞/完成三表 + 任务表 + staging 预算账）。
        /// 不含任何 Vulkan 调用；任务对象的销毁仍由本队列负责（它带着 GPU 资源）。
        /// 预算与 id 也归它 ⇒ 本队列不再另存一份（避免双份真值）。
        UploadTaskScheduler task_scheduler;

        // 跨队列同步 = **单条 upload lane**（timeline）：每次 transfer 派发 signal 一个递增值，
        // 主帧渲染提交前 wait 最新值 ⇒ 覆盖全部在飞 DMA 拷贝，且等待项恒为 1 个。
        // 旧方案（逐任务二进制信号量收集）有两处坏：① 批量完成任务的 sem 在批次执行完毕前仍
        //   “in use”，任务析构 delete → vkDestroySemaphore 踩 VUID-vkDestroySemaphore-semaphore-05149；
        // ② 收集数量可超主帧 waits[MAX_WAITS]，溢出丢弃 = 无等待裸用刚拷贝的纹理。
        Semaphore * upload_lane_       = nullptr;
        uint64_t    upload_lane_value_ = 0;      ///< 已提交的最新信号值（0 = 从未派发）

        Mutex         mutex_;

        /// 关机标记镜像：析构开始后拒绝新入队，避免 on_complete 回调里重建任务后无人回收。
        /// 真值在 `task_scheduler`（调度侧判据），这里保留名字以免调用点大面积改名。

    private:
        /**
         * 派发结论。三态而不是 bool 的原因：`false` 过去同时表示
         * 「队列暂时忙，下帧再试」与「任务参数/资源致命错误」，
         * 而调用方把两者都当成"丢掉"，任务既不在队列也不在在飞表里，
         * 上层（几何/纹理流送）永远收不到 on_complete ⇒ 状态永久停在 Loading。
         */
        enum class DispatchOutcome : uint8_t
        {
            Submitted,  ///< 已提交（或已同步完成）⇒ 任务归在飞/完成表，不得再动
            Deferred,   ///< 队列暂时不可派发（前次提交未完成）⇒ 放回队列下轮重试
            Failed      ///< 致命错误 ⇒ 必须走终态失败清理并回调 on_complete
        };

        void EnsureResources();
        DispatchOutcome DispatchTask(GpuUploadTask *task);
        DispatchOutcome DispatchGpuTransferDma(GpuUploadTask *task);
        DispatchOutcome DispatchGpuGraphicsDma(GpuUploadTask *task);
        DispatchOutcome DispatchBufferToBuffer(GpuUploadTask *task);

        /** 致命失败终态：退还 staging 记账、回调 on_complete、摘出任务表并销毁 */
        void FailTask(GpuUploadTask *task, const char8_t *reason);
        /** 暂时不可派发：放回对应优先级队列头部，下一轮 Update 重试 */
        void DeferTask(GpuUploadTask *task);

    public:
        GpuUploadQueue(VulkanDevice *device);
        ~GpuUploadQueue();

        /** 设置 Staging 显存上限预算（字节） */
        void SetMaxStagingMemory(VkDeviceSize max_bytes) { task_scheduler.SetMaxStagingBytes(max_bytes); }
        VkDeviceSize GetMaxStagingMemory() const { return task_scheduler.GetMaxStagingBytes(); }
        VkDeviceSize GetCurrentStagingMemory() const { return task_scheduler.GetCurrentStagingBytes(); }

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
