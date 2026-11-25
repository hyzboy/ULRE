#include <hgl/graph/module/GpuUploadQueue.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKQueue.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/vk/VKSemaphore.h>
#include <hgl/vk/VKFence.h>
#include <hgl/vk/VKDeviceAttribute.h>
#include <hgl/vk/VKPhysicalDevice.h>
#include <hgl/vk/VKTexture.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <hgl/type/Smart.h>
#include <hgl/time/Time.h>
#include <hgl/log/Log.h>

namespace hgl::graph
{
    void GenerateMipmaps(TextureCmdBuffer *texture_cmd_buf,
                         VkImage image,
                         VkImageAspectFlags aspect_mask,
                         VkExtent3D extent,
                         const uint32_t mipLevels,
                         const uint32_t base_array_layer,
                         const uint32_t layer_count);

    class StagingRingBuffer
    {
    public:
        static constexpr VkDeviceSize RING_ALIGNMENT = 256;

        struct Block
        {
            uint64_t     task_id  = 0;
            VkDeviceSize offset   = 0;
            VkDeviceSize size     = 0;
            bool         wrapped  = false;
            bool         freed    = false;

            bool operator==(const Block &o) const
            {
                return task_id == o.task_id && offset == o.offset;
            }
        };

    private:
        VulkanDevice *      device_          = nullptr;
        DeviceBuffer *      buffer_          = nullptr;
        uint8_t *           mapped_ptr_      = nullptr;
        VkDeviceSize        capacity_        = 0;
        VkDeviceSize        head_            = 0;
        VkDeviceSize        tail_            = 0;
        VkDeviceSize        allocated_bytes_ = 0;
        ValueArray<Block>   active_blocks_;

    public:
        StagingRingBuffer() = default;
        ~StagingRingBuffer() { Cleanup(); }

        bool Init(VulkanDevice *device, VkDeviceSize size)
        {
            Cleanup();
            if (!device || size == 0)
                return false;

            device_ = device;
            capacity_ = size;
            buffer_ = device_->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, capacity_);
            if (!buffer_)
                return false;

            mapped_ptr_ = static_cast<uint8_t *>(buffer_->Map());
            if (!mapped_ptr_)
            {
                delete buffer_;
                buffer_ = nullptr;
                return false;
            }

            head_ = 0;
            tail_ = 0;
            allocated_bytes_ = 0;
            return true;
        }

        void Cleanup()
        {
            if (buffer_)
            {
                buffer_->Unmap();
                delete buffer_;
                buffer_ = nullptr;
            }
            mapped_ptr_ = nullptr;
            head_ = 0;
            tail_ = 0;
            allocated_bytes_ = 0;
            active_blocks_.Clear();
            device_ = nullptr;
        }

        bool Allocate(uint64_t task_id, VkDeviceSize size, VkDeviceSize &out_offset, void *&out_mapped_ptr)
        {
            if (!buffer_ || !mapped_ptr_ || size == 0)
                return false;

            // 单次贴图尺寸若超过环容量一半，回退至独立 StagingBuffer
            if (size > (capacity_ / 2))
                return false;

            const VkDeviceSize aligned_size = (size + (RING_ALIGNMENT - 1)) & ~(RING_ALIGNMENT - 1);

            if (active_blocks_.IsEmpty())
            {
                head_ = 0;
                tail_ = 0;
                allocated_bytes_ = 0;

                out_offset = 0;
                out_mapped_ptr = mapped_ptr_;
                head_ = aligned_size;
                allocated_bytes_ = aligned_size;

                Block b;
                b.task_id = task_id;
                b.offset = 0;
                b.size = aligned_size;
                b.wrapped = false;
                b.freed = false;
                active_blocks_.Add(b);
                return true;
            }

            if (head_ >= tail_)
            {
                if (head_ + aligned_size <= capacity_)
                {
                    out_offset = head_;
                    out_mapped_ptr = mapped_ptr_ + head_;
                    head_ += aligned_size;
                    allocated_bytes_ += aligned_size;

                    Block b;
                    b.task_id = task_id;
                    b.offset = out_offset;
                    b.size = aligned_size;
                    b.wrapped = false;
                    b.freed = false;
                    active_blocks_.Add(b);
                    return true;
                }

                if (tail_ > aligned_size)
                {
                    out_offset = 0;
                    out_mapped_ptr = mapped_ptr_;
                    head_ = aligned_size;
                    allocated_bytes_ += aligned_size;

                    Block b;
                    b.task_id = task_id;
                    b.offset = 0;
                    b.size = aligned_size;
                    b.wrapped = true;
                    b.freed = false;
                    active_blocks_.Add(b);
                    return true;
                }

                return false;
            }
            else
            {
                if (head_ + aligned_size < tail_)
                {
                    out_offset = head_;
                    out_mapped_ptr = mapped_ptr_ + head_;
                    head_ += aligned_size;
                    allocated_bytes_ += aligned_size;

                    Block b;
                    b.task_id = task_id;
                    b.offset = out_offset;
                    b.size = aligned_size;
                    b.wrapped = false;
                    b.freed = false;
                    active_blocks_.Add(b);
                    return true;
                }

                return false;
            }
        }

        void Free(uint64_t task_id)
        {
            for (int i = 0; i < active_blocks_.GetCount(); ++i)
            {
                if (active_blocks_[i].task_id == task_id)
                {
                    active_blocks_[i].freed = true;
                    break;
                }
            }

            while (!active_blocks_.IsEmpty() && active_blocks_[0].freed)
            {
                const Block &b = active_blocks_[0];
                if (allocated_bytes_ >= b.size)
                    allocated_bytes_ -= b.size;
                else
                    allocated_bytes_ = 0;

                if (b.wrapped)
                    tail_ = b.size;
                else
                    tail_ = b.offset + b.size;

                active_blocks_.Delete(0);
            }

            if (active_blocks_.IsEmpty())
            {
                head_ = 0;
                tail_ = 0;
                allocated_bytes_ = 0;
            }
        }

        DeviceBuffer *GetBuffer() const { return buffer_; }
        bool          IsInitialized() const { return buffer_ != nullptr; }
    };

    GpuUploadQueue::GpuUploadQueue(VulkanDevice *device)
        : device_(device)
    {
        EnsureResources();
    }

    GpuUploadQueue::~GpuUploadQueue()
    {
        task_scheduler.BeginShutdown();

        if (device_)
            device_->WaitIdle();

        // ① 先上报取消。析构路径原本直接 delete 任务而不走完成分支 ⇒ 上层的在途作业
        //    （几何 staging 缓冲、handler 引用、流送计数）永远收不到终态：既泄漏又让
        //    调用方状态永久停在 Loading。这里统一以 Cancelled 通知一次。
        //    完成表不再通知 —— 它们的 on_complete 在完成时已调用过。
        for (int p = 0; p < static_cast<int>(UploadPriority::Count); ++p)
        {
            for (int i = 0; i < task_scheduler.QueuedTasks(size_t(p)).GetCount(); ++i)
            {
                GpuUploadTask *task = task_scheduler.QueuedTasks(size_t(p))[i];
                if (!task)
                    continue;

                task->state = UploadTaskState::Cancelled;
                if (task->on_complete)
                    task->on_complete(task, task->user_data);
            }
        }

        for (int i = 0; i < task_scheduler.InFlightTasks().GetCount(); ++i)
        {
            GpuUploadTask *task = task_scheduler.InFlightTasks()[i];
            if (!task)
                continue;

            task->state = UploadTaskState::Cancelled;
            if (task->on_complete)
                task->on_complete(task, task->user_data);
        }

        // ② 释放设备侧辅助对象（WaitIdle 已保证无批次还引用它们）
        SAFE_CLEAR(transfer_ring_);
        SAFE_CLEAR(graphics_ring_);
        SAFE_CLEAR(transfer_cmd_buf_);
        SAFE_CLEAR(graphics_cmd_buf_);
        SAFE_CLEAR(transfer_queue_);
        SAFE_CLEAR(graphics_queue_);
        SAFE_CLEAR(upload_lane_);   // device_->WaitIdle() 已执行 ⇒ 无任何批次还引用它

        // ③ 释放在册任务（环形 staging 已随手销毁，任务只持有自己的缓冲）
        for (int p = 0; p < static_cast<int>(UploadPriority::Count); ++p)
        {
            for (int i = 0; i < task_scheduler.QueuedTasks(size_t(p)).GetCount(); ++i)
            {
                delete task_scheduler.QueuedTasks(size_t(p))[i];
            }
            task_scheduler.QueuedTasks(size_t(p)).Clear();
        }

        for (int i = 0; i < task_scheduler.InFlightTasks().GetCount(); ++i)
        {
            delete task_scheduler.InFlightTasks()[i];
        }
        task_scheduler.InFlightTasks().Clear();

        for (int i = 0; i < task_scheduler.CompletedTasks().GetCount(); ++i)
        {
            delete task_scheduler.CompletedTasks()[i];
        }
        task_scheduler.CompletedTasks().Clear();

        task_scheduler.ClearTaskTable();
    }

    void GpuUploadQueue::EnsureResources()
    {
        if (!device_)
            return;

        if (!transfer_ring_)
        {
            transfer_ring_ = new StagingRingBuffer();
            transfer_ring_->Init(device_, 16ULL * 1024 * 1024);
        }
        if (!graphics_ring_)
        {
            graphics_ring_ = new StagingRingBuffer();
            graphics_ring_->Init(device_, 16ULL * 1024 * 1024);
        }

        if (!transfer_queue_)
        {
            transfer_queue_ = device_->CreateTransferQueue("GpuUploadTransferQueue", 2);
        }
        if (!transfer_cmd_buf_)
        {
            transfer_cmd_buf_ = device_->CreateTransferTextureCommandBuffer("GpuUploadTransferCmdBuf");
        }
        if (!graphics_queue_)
        {
            graphics_queue_ = device_->CreateQueue("GpuUploadGraphicsQueue", 2);
        }
        if (!graphics_cmd_buf_)
        {
            graphics_cmd_buf_ = device_->CreateTextureCommandBuffer("GpuUploadGraphicsCmdBuf");
        }
        if (!upload_lane_)
        {
            upload_lane_ = device_->CreateTimelineSemaphore("GpuUploadLane");
        }
    }

    uint64_t GpuUploadQueue::Enqueue(GpuUploadTask *task)
    {
        if (!task)
            return 0;

        MutexLock lock(&mutex_);

        // 关机中：拒绝新任务，但仍以 Cancelled 回调通知调用方（否则上层拿不到终态）。
        if (task_scheduler.IsShuttingDown())
        {
            if (task->on_complete)
            {
                task->state = UploadTaskState::Cancelled;
                task->on_complete(task, task->user_data);
            }
            LogWarning(u8"[GpuUploadQueue] Enqueue rejected during shutdown");
            return 0;
        }

        const uint64_t task_id = task_scheduler.NextTaskID();
        task->task_id = task_id;
        task->state = UploadTaskState::Queued;

        // 自动判定后端（仅在 GPU 专用 Transfer DMA 与 Graphics DMA 之间选择）
        if (task->backend == UploadBackendType::Auto)
        {
            if (task->kind == GpuUploadKind::BufferToBuffer)
            {
                // 纯字节拷贝无需 Blit：有专用 Transfer 队列就走 DMA，否则回落 Graphics 队列
                task->backend = (device_ && device_->GetTransferFamilyIndex() != VK_QUEUE_FAMILY_IGNORED)
                    ? UploadBackendType::GpuTransferDma : UploadBackendType::GpuGraphicsDma;
            }
            else if (device_ && device_->GetTransferFamilyIndex() != VK_QUEUE_FAMILY_IGNORED &&
                !task->auto_mipmaps && task->priority != UploadPriority::Immediate)
            {
                task->backend = UploadBackendType::GpuTransferDma;
            }
            else
            {
                task->backend = UploadBackendType::GpuGraphicsDma;
            }
        }

        task_scheduler.Register(task);

        // Immediate 任务直接同步派发并等待完成
        if (task->priority == UploadPriority::Immediate)
        {
            // 派发结论必须处理：旧实现忽略返回值，失败时任务既不在队列也不在在飞表，
            // 调用方的 on_complete 永不到达 ⇒ 永久停在 Loading。
            switch (DispatchTask(task))
            {
                case DispatchOutcome::Submitted:
                    break;
                case DispatchOutcome::Deferred:
                    DeferTask(task);
                    break;
                case DispatchOutcome::Failed:
                    FailTask(task, u8"immediate dispatch failed");
                    break;
            }
            return task_id;
        }

        task_scheduler.PushQueued(task);
        return task_id;
    }

    bool GpuUploadQueue::CancelUpload(uint64_t task_id)
    {
        MutexLock lock(&mutex_);
        return task_scheduler.Cancel(task_id);
    }

    UploadTaskState GpuUploadQueue::GetTaskState(uint64_t task_id)
    {
        MutexLock lock(&mutex_);
        return task_scheduler.State(task_id);
    }

    GpuUploadTask *GpuUploadQueue::FindTask(uint64_t task_id)
    {
        MutexLock lock(&mutex_);
        return task_scheduler.Find(task_id);
    }

    void GpuUploadQueue::WaitTask(uint64_t task_id)
    {
        GpuUploadTask *task = FindTask(task_id);
        if (!task)
            return;

        if (task->state == UploadTaskState::Queued)
        {
            MutexLock lock(&mutex_);
            task_scheduler.RemoveFromQueues(task);

            // 同步派发：队列暂时忙时自旋等待（有界），致命错误走终态失败回调。
            // 旧实现忽略返回值 ⇒ 失败后任务既不在队列也不在在飞表，调用方永久等待。
            bool submitted = false;
            for (int spin = 0; spin < 5000; ++spin)
            {
                const DispatchOutcome outcome = DispatchTask(task);

                if (outcome == DispatchOutcome::Submitted)
                {
                    submitted = true;
                    break;
                }
                if (outcome == DispatchOutcome::Failed)
                {
                    FailTask(task, u8"sync dispatch failed");
                    return;
                }

                hgl::SleepSecond(0.001);
            }

            if (!submitted)
            {
                // 自旋超限仍未派出：放回队列，交给下一轮 Update 继续，绝不丢任务。
                LogWarning(u8"[GpuUploadQueue] WaitTask could not dispatch task=%llu synchronously; deferring",
                           static_cast<unsigned long long>(task->task_id));
                DeferTask(task);
                return;
            }
        }

        if (task->state == UploadTaskState::InFlight)
        {
            if (task->backend == UploadBackendType::GpuTransferDma)
            {
                if(!transfer_queue_)
                {
                    LogError(u8"[GpuUploadQueue] CompleteTask has no transfer queue for in-flight task=%llu",
                             static_cast<unsigned long long>(task->task_id));
                    return;
                }
                if(!transfer_queue_->WaitLastSubmitFence()&&!transfer_queue_->WaitQueue())
                {
                    LogError(u8"[GpuUploadQueue] CompleteTask could not drain transfer queue task=%llu",
                             static_cast<unsigned long long>(task->task_id));
                    return;
                }
            }
            else if (graphics_queue_)
            {
                if(!graphics_queue_->WaitLastSubmitFence()&&!graphics_queue_->WaitQueue())
                {
                    LogError(u8"[GpuUploadQueue] CompleteTask could not drain graphics queue task=%llu",
                             static_cast<unsigned long long>(task->task_id));
                    return;
                }
            }
            else
            {
                LogError(u8"[GpuUploadQueue] CompleteTask has no queue for in-flight task=%llu",
                         static_cast<unsigned long long>(task->task_id));
                return;
            }

            if (task->staging_buffer)
            {
                task_scheduler.ReleaseStagingBytes(task->staging_bytes);
                if (!task->is_ring_staging && task->owns_staging)
                    delete task->staging_buffer;
                task->staging_buffer = nullptr;
            }

            if (task->is_ring_staging)
            {
                if (task->backend == UploadBackendType::GpuTransferDma && transfer_ring_)
                    transfer_ring_->Free(task->task_id);
                else if (graphics_ring_)
                    graphics_ring_->Free(task->task_id);
            }

            task->state = UploadTaskState::Completed;
            if (task->on_complete)
                task->on_complete(task, task->user_data);
        }
    }

    GpuUploadQueue::DispatchOutcome GpuUploadQueue::DispatchTask(GpuUploadTask *task)
    {
        if (!task)
            return DispatchOutcome::Failed;

        // 缓冲支路（几何/SSBO 等原始字节）与载荷无关，统一走 CopyBuffer 派发
        if (task->kind == GpuUploadKind::BufferToBuffer)
            return DispatchBufferToBuffer(task);

        switch (task->backend)
        {
            case UploadBackendType::GpuTransferDma:
                return DispatchGpuTransferDma(task);
            case UploadBackendType::GpuGraphicsDma:
            default:
                return DispatchGpuGraphicsDma(task);
        }
    }

    void GpuUploadQueue::FailTask(GpuUploadTask *task, const char8_t *reason)
    {
        if (!task)
            return;

        LogError(u8"[GpuUploadQueue] task %llu failed permanently: %s",
                 static_cast<unsigned long long>(task->task_id),
                 reason ? reason : u8"unknown");

        // 退还 staging 记账。只有真正挂上了 staging 缓冲的任务才被记过账
        // （各派发路径都在 staging 就位之后才 += staging_bytes）。
        if (task->staging_buffer)
        {
            task_scheduler.ReleaseStagingBytes(task->staging_bytes);
            if (task->is_ring_staging)
            {
                if (task->backend == UploadBackendType::GpuTransferDma && transfer_ring_)
                    transfer_ring_->Free(task->task_id);
                else if (graphics_ring_)
                    graphics_ring_->Free(task->task_id);
            }
            else if (task->owns_staging)
            {
                delete task->staging_buffer;
            }
            task->staging_buffer = nullptr;
        }

        task->state = UploadTaskState::Cancelled;
        if (task->on_complete)
            task->on_complete(task, task->user_data);

        task_scheduler.Unregister(task);
        delete task;
    }

    void GpuUploadQueue::DeferTask(GpuUploadTask *task)
    {
        if (!task)
            return;

        const size_t priority = static_cast<size_t>(task->priority);
        const size_t bucket = priority < static_cast<size_t>(UploadPriority::Count)
                            ? priority : static_cast<size_t>(UploadPriority::Normal);

        // 放回队首：保持同优先级内的原始先后次序，并保证下一轮 Update 必定重试。
        task_scheduler.DeferToHead(task);
    }

    GpuUploadQueue::DispatchOutcome GpuUploadQueue::DispatchGpuTransferDma(GpuUploadTask *task)
    {
        if (!task || !task->tci || !task->target_texture || !device_)
            return DispatchOutcome::Failed;

        EnsureResources();

        if(!transfer_queue_||!transfer_cmd_buf_)
        {
            LogError(u8"[GpuUploadQueue] GpuTransferDma missing queue or command buffer");
            return DispatchOutcome::Failed;
        }
        if(!transfer_queue_->WaitLastSubmitFence())
        {
            // 上一批 DMA 仍在执行 ⇒ 只是"暂时不可派发"，任务必须留在队列里下轮重试，
            // 不能当作失败丢弃（否则上层回调永不到达）。
            LogDebug(u8"[GpuUploadQueue] GpuTransferDma prior submission still in flight; retry next update");
            return DispatchOutcome::Deferred;
        }

        TextureCreateInfo *tci = task->tci;
        Texture *tex = task->target_texture;
        VkImage image = tex->GetImage();
        if (image == VK_NULL_HANDLE)
            return DispatchOutcome::Failed;

        const VkDeviceSize total_bytes = tci->total_bytes > 0 ? tci->total_bytes : (tci->buffer ? tci->buffer->GetSize() : 0);
        if (total_bytes == 0)
        {
            LogError(u8"[GpuUploadQueue] GpuTransferDma failed: zero total bytes");
            return DispatchOutcome::Failed;
        }

        task->staging_bytes = total_bytes;

        if (tci->buffer)
        {
            // 零拷贝直通：直接接管 tci->buffer 作为 Transfer 源缓冲，消除额外的 CPU 内存分配与 memcpy
            task->staging_buffer = tci->buffer;
            tci->buffer = nullptr;
            task->is_ring_staging = false;
            task->ring_offset = tci->data_offset;   // Direct-to-Staging：像素可从缓冲内偏移处开始(如文件头之后)
        }
        else if (tci->pixels)
        {
            void *mapped_ptr = nullptr;
            VkDeviceSize ring_offset = 0;
            if (transfer_ring_ && transfer_ring_->Allocate(task->task_id, total_bytes, ring_offset, mapped_ptr))
            {
                memcpy(mapped_ptr, tci->pixels, total_bytes);
                task->staging_buffer = transfer_ring_->GetBuffer();
                task->is_ring_staging = true;
                task->ring_offset = ring_offset;
            }
            else
            {
                task->staging_buffer = device_->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, total_bytes);
                if (!task->staging_buffer)
                    return DispatchOutcome::Failed;

                task->is_ring_staging = false;
                task->ring_offset = 0;

                void *dst = task->staging_buffer->Map();
                if (dst)
                {
                    memcpy(dst, tci->pixels, total_bytes);
                    task->staging_buffer->Unmap();
                }
            }
        }
        else
        {
            LogError(u8"[GpuUploadQueue] GpuTransferDma failed: no pixel data or buffer");
            return DispatchOutcome::Failed;
        }

        task_scheduler.AddStagingBytes(task->staging_bytes);

        const uint32_t mip_levels = tci->target_mipmaps > 0 ? tci->target_mipmaps : 1;

        if(!transfer_cmd_buf_->Begin())
        {
            LogError(u8"[GpuUploadQueue] GpuTransferDma command buffer begin failed task=%llu",
                     static_cast<unsigned long long>(task->task_id));
            return DispatchOutcome::Failed;
        }

        VkImageSubresourceRange subresource_range{};
        subresource_range.aspectMask = tex->GetAspect();
        subresource_range.baseMipLevel = 0;
        subresource_range.levelCount = mip_levels;
        subresource_range.baseArrayLayer = 0;
        subresource_range.layerCount = 1;

        transfer_cmd_buf_->ImageMemoryBarrier2(
            image,
            VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresource_range);

        AutoDeleteArray<VkBufferImageCopy> bic_list(mip_levels);
        bic_list.zero();
        VkDeviceSize offset = 0;
        uint32_t width = tci->extent.width;
        uint32_t height = tci->extent.height;
        uint32_t rolling_level_bytes = tci->mipmap_zero_total_bytes;

        for (uint32_t level = 0; level < mip_levels; ++level)
        {
            VkBufferImageCopy &bic = bic_list[level];
            bic.bufferOffset = task->ring_offset + offset;
            bic.bufferRowLength = 0;
            bic.bufferImageHeight = 0;
            bic.imageSubresource.aspectMask = tex->GetAspect();
            bic.imageSubresource.mipLevel = level;
            bic.imageSubresource.baseArrayLayer = 0;
            bic.imageSubresource.layerCount = 1;
            bic.imageOffset = {0, 0, 0};
            bic.imageExtent.width = width;
            bic.imageExtent.height = height;
            bic.imageExtent.depth = 1;

            const bool can_half_width = (width > 1);
            const bool can_half_height = (height > 1);

            uint32_t level_bytes = 0;
            if (IsBlockCompressedFormat(tex->GetFormat()))
            {
                if (GetBlockCompressedLevelBytes(tex->GetFormat(), width, height, level_bytes))
                    offset += level_bytes;
            }
            else
            {
                if (rolling_level_bytes < 8)
                    offset += 8;
                else
                    offset += rolling_level_bytes;

                if (can_half_width) rolling_level_bytes >>= 1;
                if (can_half_height) rolling_level_bytes >>= 1;
            }

            if (can_half_width) width >>= 1;
            if (can_half_height) height >>= 1;
        }

        transfer_cmd_buf_->CopyBufferToImage(task->staging_buffer->GetBuffer(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mip_levels, bic_list.data());

        transfer_cmd_buf_->ImageMemoryBarrier2(
            image,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_NONE,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            subresource_range);

        if(!transfer_cmd_buf_->End())
        {
            LogError(u8"[GpuUploadQueue] GpuTransferDma command buffer end failed task=%llu",
                     static_cast<unsigned long long>(task->task_id));
            return DispatchOutcome::Failed;
        }

        tex->SetImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // upload lane：signal 递增值（sem 为 null 时提交路径自动跳过 signal 项）；
        // 提交成功后才抬升对外的值 ⇒ 等待方看到的每个值都已真正入队
        const uint64_t lane_signal = upload_lane_ ? upload_lane_->NextValue() : 0;
        SemaphoreSubmit transfer_signal = SemaphoreSubmit::Signal(upload_lane_, lane_signal);
        if(!transfer_queue_->Submit(transfer_cmd_buf_, nullptr, 0, &transfer_signal, 1))
        {
            LogError(u8"[GpuUploadQueue] GpuTransferDma submit failed task=%llu",
                     static_cast<unsigned long long>(task->task_id));
            return DispatchOutcome::Failed;
        }
        if(lane_signal!=0)
            upload_lane_value_=lane_signal;

        task->state = UploadTaskState::InFlight;
        task_scheduler.MarkInFlight(task);

        if (task->priority == UploadPriority::Immediate)
        {
            if(!transfer_queue_->WaitLastSubmitFence()&&!transfer_queue_->WaitQueue())
            {
                // 同步等待失败不是派发失败：任务已在在飞表，Update() 会按完成状态收尾。
                // 绝不能在这里丢弃任务（旧实现直接 return false ⇒ 任务永久滞留在在飞表
                // 且调用方以为失败）。
                LogWarning(u8"[GpuUploadQueue] Immediate transfer completion wait failed task=%llu; leaving it in flight",
                           static_cast<unsigned long long>(task->task_id));
                return DispatchOutcome::Submitted;
            }
            task_scheduler.ReleaseStagingBytes(task->staging_bytes);
            if (!task->is_ring_staging && task->owns_staging && task->staging_buffer)
            {
                delete task->staging_buffer;
            }
            task->staging_buffer = nullptr;

            if (task->is_ring_staging && transfer_ring_)
            {
                transfer_ring_->Free(task->task_id);
            }

            task->state = UploadTaskState::Completed;
            if (task->on_complete)
                task->on_complete(task, task->user_data);

            task_scheduler.RemoveFromInFlight(task);
            task_scheduler.PushCompleted(task);
        }

        return DispatchOutcome::Submitted;
    }

    GpuUploadQueue::DispatchOutcome GpuUploadQueue::DispatchGpuGraphicsDma(GpuUploadTask *task)
    {
        if (!task || !task->tci || !task->target_texture || !device_)
            return DispatchOutcome::Failed;

        EnsureResources();

        if(!graphics_queue_||!graphics_cmd_buf_)
        {
            LogError(u8"[GpuUploadQueue] GpuGraphicsDma missing queue or command buffer");
            return DispatchOutcome::Failed;
        }
        if(!graphics_queue_->WaitLastSubmitFence())
        {
            LogDebug(u8"[GpuUploadQueue] GpuGraphicsDma prior submission still in flight; retry next update");
            return DispatchOutcome::Deferred;
        }

        TextureCreateInfo *tci = task->tci;
        Texture *tex = task->target_texture;
        VkImage image = tex->GetImage();
        if (image == VK_NULL_HANDLE)
            return DispatchOutcome::Failed;

        const VkDeviceSize total_bytes = tci->total_bytes > 0 ? tci->total_bytes : (tci->buffer ? tci->buffer->GetSize() : 0);
        if (total_bytes == 0)
        {
            LogError(u8"[GpuUploadQueue] GpuGraphicsDma failed: zero total bytes");
            return DispatchOutcome::Failed;
        }

        task->staging_bytes = total_bytes;

        if (tci->buffer)
        {
            // 零拷贝直通：直接接管 tci->buffer 作为 Transfer 源缓冲，消除额外的 CPU 内存分配与 memcpy
            task->staging_buffer = tci->buffer;
            tci->buffer = nullptr;
            task->is_ring_staging = false;
            task->ring_offset = tci->data_offset;   // Direct-to-Staging：像素可从缓冲内偏移处开始(如文件头之后)
        }
        else if (tci->pixels)
        {
            void *mapped_ptr = nullptr;
            VkDeviceSize ring_offset = 0;
            if (graphics_ring_ && graphics_ring_->Allocate(task->task_id, total_bytes, ring_offset, mapped_ptr))
            {
                memcpy(mapped_ptr, tci->pixels, total_bytes);
                task->staging_buffer = graphics_ring_->GetBuffer();
                task->is_ring_staging = true;
                task->ring_offset = ring_offset;
            }
            else
            {
                task->staging_buffer = device_->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, total_bytes);
                if (!task->staging_buffer)
                    return DispatchOutcome::Failed;

                task->is_ring_staging = false;
                task->ring_offset = 0;

                void *dst = task->staging_buffer->Map();
                if (dst)
                {
                    memcpy(dst, tci->pixels, total_bytes);
                    task->staging_buffer->Unmap();
                }
            }
        }
        else
        {
            LogError(u8"[GpuUploadQueue] GpuGraphicsDma failed: no pixel data or buffer");
            return DispatchOutcome::Failed;
        }

        task_scheduler.AddStagingBytes(task->staging_bytes);

        const uint32_t mip_levels = tci->target_mipmaps > 0 ? tci->target_mipmaps : 1;

        if(!graphics_cmd_buf_->Begin())
        {
            LogError(u8"[GpuUploadQueue] GpuGraphicsDma command buffer begin failed task=%llu",
                     static_cast<unsigned long long>(task->task_id));
            return DispatchOutcome::Failed;
        }

        VkImageSubresourceRange subresource_range{};
        subresource_range.aspectMask = tex->GetAspect();
        subresource_range.baseMipLevel = 0;
        subresource_range.levelCount = mip_levels;
        subresource_range.baseArrayLayer = 0;
        subresource_range.layerCount = 1;

        graphics_cmd_buf_->ImageMemoryBarrier2(
            image,
            VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresource_range);

        if (task->auto_mipmaps)
        {
            // 只拷贝 Level 0，然后 Blit 生成后续 Mipmaps
            VkBufferImageCopy bic{};
            bic.bufferOffset = task->ring_offset;
            bic.bufferRowLength = 0;
            bic.bufferImageHeight = 0;
            bic.imageSubresource.aspectMask = tex->GetAspect();
            bic.imageSubresource.mipLevel = 0;
            bic.imageSubresource.baseArrayLayer = 0;
            bic.imageSubresource.layerCount = 1;
            bic.imageOffset = {0, 0, 0};
            bic.imageExtent = tci->extent;

            graphics_cmd_buf_->CopyBufferToImage(task->staging_buffer->GetBuffer(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bic);
            GenerateMipmaps(graphics_cmd_buf_, image, tex->GetAspect(), tci->extent, mip_levels, 0, 1);
        }
        else
        {
            AutoDeleteArray<VkBufferImageCopy> bic_list(mip_levels);
            bic_list.zero();
            VkDeviceSize offset = 0;
            uint32_t width = tci->extent.width;
            uint32_t height = tci->extent.height;
            uint32_t rolling_level_bytes = tci->mipmap_zero_total_bytes;

            for (uint32_t level = 0; level < mip_levels; ++level)
            {
                VkBufferImageCopy &bic = bic_list[level];
                bic.bufferOffset = task->ring_offset + offset;
                bic.bufferRowLength = 0;
                bic.bufferImageHeight = 0;
                bic.imageSubresource.aspectMask = tex->GetAspect();
                bic.imageSubresource.mipLevel = level;
                bic.imageSubresource.baseArrayLayer = 0;
                bic.imageSubresource.layerCount = 1;
                bic.imageOffset = {0, 0, 0};
                bic.imageExtent.width = width;
                bic.imageExtent.height = height;
                bic.imageExtent.depth = 1;

                const bool can_half_width = (width > 1);
                const bool can_half_height = (height > 1);

                uint32_t level_bytes = 0;
                if (IsBlockCompressedFormat(tex->GetFormat()))
                {
                    if (GetBlockCompressedLevelBytes(tex->GetFormat(), width, height, level_bytes))
                        offset += level_bytes;
                }
                else
                {
                    if (rolling_level_bytes < 8)
                        offset += 8;
                    else
                        offset += rolling_level_bytes;

                    if (can_half_width) rolling_level_bytes >>= 1;
                    if (can_half_height) rolling_level_bytes >>= 1;
                }

                if (can_half_width) width >>= 1;
                if (can_half_height) height >>= 1;
            }

            graphics_cmd_buf_->CopyBufferToImage(task->staging_buffer->GetBuffer(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mip_levels, bic_list.data());
        }

        if (!task->auto_mipmaps)
        {
            // 非自动生成路径：全部 mip 层由拷贝写入，统一停在 TRANSFER_DST
            graphics_cmd_buf_->ImageMemoryBarrier2(
                image,
                VK_PIPELINE_STAGE_2_COPY_BIT,
                VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_ACCESS_2_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                subresource_range);
        }
        // 自动生成路径：GenerateMipmaps 内部已逐层把 0..mip_levels-1 转换到 SHADER_READ_ONLY，
        // 这里若再以 TRANSFER_DST 为 oldLayout 整体转换一次，会与已记录的布局冲突(VUID 01197)。

        if(!graphics_cmd_buf_->End())
        {
            LogError(u8"[GpuUploadQueue] GpuGraphicsDma command buffer end failed task=%llu",
                     static_cast<unsigned long long>(task->task_id));
            return DispatchOutcome::Failed;
        }

        tex->SetImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        if(!graphics_queue_->Submit(graphics_cmd_buf_, nullptr, 0, nullptr, 0))
        {
            LogError(u8"[GpuUploadQueue] GpuGraphicsDma submit failed task=%llu",
                     static_cast<unsigned long long>(task->task_id));
            return DispatchOutcome::Failed;
        }

        task->state = UploadTaskState::InFlight;
        task_scheduler.MarkInFlight(task);

        if (task->priority == UploadPriority::Immediate)
        {
            if(!graphics_queue_->WaitLastSubmitFence()&&!graphics_queue_->WaitQueue())
            {
                LogWarning(u8"[GpuUploadQueue] Immediate graphics completion wait failed task=%llu; leaving it in flight",
                           static_cast<unsigned long long>(task->task_id));
                return DispatchOutcome::Submitted;
            }
            task_scheduler.ReleaseStagingBytes(task->staging_bytes);
            if (!task->is_ring_staging && task->owns_staging && task->staging_buffer)
            {
                delete task->staging_buffer;
            }
            task->staging_buffer = nullptr;

            if (task->is_ring_staging && graphics_ring_)
            {
                graphics_ring_->Free(task->task_id);
            }

            task->state = UploadTaskState::Completed;
            if (task->on_complete)
                task->on_complete(task, task->user_data);

            task_scheduler.RemoveFromInFlight(task);
            task_scheduler.PushCompleted(task);
        }

        return DispatchOutcome::Submitted;
    }

    uint64_t GpuUploadQueue::EnqueueBufferCopyRegions(DeviceBuffer *staging, BufferOwner *target,
        const ValueArray<BufferCopyRegion> &regions, UploadPriority priority,
        void (*on_complete)(GpuUploadTask *, void *), void *user_data, bool owns_staging)
    {
        if (!staging || !target || regions.IsEmpty())
        {
            LogError(u8"[GpuUploadQueue] EnqueueBufferCopyRegions: invalid args");
            return 0;
        }

        GpuUploadTask *task = new GpuUploadTask();
        task->kind           = GpuUploadKind::BufferToBuffer;
        task->priority       = priority;
        task->backend        = UploadBackendType::Auto;
        task->staging_buffer = staging;
        task->owns_staging   = owns_staging;
        task->is_ring_staging = false;
        task->target_buffer  = target;
        for (int i = 0; i < regions.GetCount(); ++i)
            task->copy_regions.Add(regions[i]);

        VkDeviceSize total = 0;
        for (int i = 0; i < regions.GetCount(); ++i)
            total += regions[i].size;
        task->staging_bytes = total;

        return Enqueue(task);
    }

    uint64_t GpuUploadQueue::EnqueueBufferCopy(DeviceBuffer *staging, VkDeviceSize src_offset,
        BufferOwner *target, VkDeviceSize dst_offset, VkDeviceSize size, UploadPriority priority,
        void (*on_complete)(GpuUploadTask *, void *), void *user_data, bool owns_staging)
    {
        BufferCopyRegion region;
        region.src_offset = src_offset;
        region.dst_offset = dst_offset;
        region.size       = size;

        ValueArray<BufferCopyRegion> regions;
        regions.Add(region);
        return EnqueueBufferCopyRegions(staging, target, regions, priority, on_complete, user_data, owns_staging);
    }

    GpuUploadQueue::DispatchOutcome GpuUploadQueue::DispatchBufferToBuffer(GpuUploadTask *task)
    {
        if (!task || !task->staging_buffer || !task->target_buffer || !device_)
        {
            LogError(u8"[GpuUploadQueue] BufferToBuffer dispatch: missing staging(%p) or target(%p)",
                     task ? (void *)task->staging_buffer : nullptr,
                     task ? (void *)task->target_buffer : nullptr);
            return DispatchOutcome::Failed;
        }

        EnsureResources();

        const bool via_transfer = (task->backend == UploadBackendType::GpuTransferDma);
        DeviceQueue *queue      = via_transfer ? transfer_queue_      : graphics_queue_;
        TextureCmdBuffer *cmd_buf = via_transfer ? transfer_cmd_buf_  : graphics_cmd_buf_;
        if (!queue || !cmd_buf)
        {
            LogError(u8"[GpuUploadQueue] BufferToBuffer missing %s queue or command buffer",
                     via_transfer ? u8"transfer" : u8"graphics");
            return DispatchOutcome::Failed;
        }
        if (!queue->WaitLastSubmitFence())
        {
            LogDebug(u8"[GpuUploadQueue] BufferToBuffer prior submission still in flight; retry next update");
            return DispatchOutcome::Deferred;
        }

        const VkBuffer src = task->staging_buffer->GetBuffer();
        const VkBuffer dst = task->target_buffer->GetBuffer();
        if (src == VK_NULL_HANDLE || dst == VK_NULL_HANDLE)
            return DispatchOutcome::Failed;

        const ValueArray<BufferCopyRegion> &regions = task->ResolveCopyRegions();
        const VkDeviceSize src_cap = task->staging_buffer->GetSize();
        const VkDeviceSize dst_cap = task->target_buffer->GetSize();
        VkDeviceSize region_total = 0;
        for (int i = 0; i < regions.GetCount(); ++i)
        {
            const BufferCopyRegion &r = regions[i];
            if (r.size == 0 || r.src_offset + r.size > src_cap || r.dst_offset + r.size > dst_cap)
            {
                LogError(u8"[GpuUploadQueue] BufferToBuffer region %d out of range (src=%llu+%llu/%llu dst=%llu+%llu/%llu)",
                         i,
                         (unsigned long long)r.src_offset, (unsigned long long)r.size, (unsigned long long)src_cap,
                         (unsigned long long)r.dst_offset, (unsigned long long)r.size, (unsigned long long)dst_cap);
                return DispatchOutcome::Failed;
            }
            region_total += r.size;
        }
        if (task->staging_bytes == 0)
            task->staging_bytes = region_total;

        task_scheduler.AddStagingBytes(task->staging_bytes);

        if (!cmd_buf->Begin())
        {
            LogError(u8"[GpuUploadQueue] BufferToBuffer command buffer begin failed task=%llu",
                     static_cast<unsigned long long>(task->task_id));
            return DispatchOutcome::Failed;
        }

        // 目标缓冲可见性屏障。与纹理支路的 ImageMemoryBarrier2 同一套约定：
        // 跨队列次序统一由 upload lane（timeline semaphore）保证，屏障仅描述访问/阶段依赖。
        cmd_buf->BufferMemoryBarrier2(dst,
            VK_PIPELINE_STAGE_2_NONE,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_ACCESS_2_NONE,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            0, VK_WHOLE_SIZE);

        AutoDeleteArray<VkBufferCopy> copies(regions.GetCount());
        copies.zero();
        for (int i = 0; i < regions.GetCount(); ++i)
        {
            copies[i].srcOffset = regions[i].src_offset;
            copies[i].dstOffset = regions[i].dst_offset;
            copies[i].size      = regions[i].size;
        }
        vkCmdCopyBuffer(*cmd_buf, src, dst, static_cast<uint32_t>(regions.GetCount()), copies.data());

        cmd_buf->BufferMemoryBarrier2(dst,
            VK_PIPELINE_STAGE_2_COPY_BIT,
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_TRANSFER_READ_BIT,
            0, VK_WHOLE_SIZE);

        if (!cmd_buf->End())
        {
            LogError(u8"[GpuUploadQueue] BufferToBuffer command buffer end failed task=%llu",
                     static_cast<unsigned long long>(task->task_id));
            return DispatchOutcome::Failed;
        }

        // 与纹DMA支路一致：signal upload lane，主帧使用目标缓冲前等待最新值
        const uint64_t lane_signal = upload_lane_ ? upload_lane_->NextValue() : 0;
        SemaphoreSubmit buffer_signal = SemaphoreSubmit::Signal(upload_lane_, lane_signal);
        if (!queue->Submit(cmd_buf, nullptr, 0, &buffer_signal, 1))
        {
            LogError(u8"[GpuUploadQueue] BufferToBuffer submit failed task=%llu",
                     static_cast<unsigned long long>(task->task_id));
            return DispatchOutcome::Failed;
        }
        if (lane_signal != 0)
            upload_lane_value_ = lane_signal;

        task->state = UploadTaskState::InFlight;
        task_scheduler.MarkInFlight(task);

        if (task->priority == UploadPriority::Immediate)
        {
            if (!queue->WaitLastSubmitFence() && !queue->WaitQueue())
            {
                LogWarning(u8"[GpuUploadQueue] BufferToBuffer immediate wait failed task=%llu; leaving it in flight",
                           static_cast<unsigned long long>(task->task_id));
                return DispatchOutcome::Submitted;
            }
            task_scheduler.ReleaseStagingBytes(task->staging_bytes);
            if (!task->is_ring_staging && task->owns_staging && task->staging_buffer)
                delete task->staging_buffer;
            task->staging_buffer = nullptr;

            task->state = UploadTaskState::Completed;
            if (task->on_complete)
                task->on_complete(task, task->user_data);

            task_scheduler.RemoveFromInFlight(task);
            task_scheduler.PushCompleted(task);
        }

        return DispatchOutcome::Submitted;
    }

    void GpuUploadQueue::Update()
    {
        MutexLock lock(&mutex_);

        // 1. 检查已在飞任务的完成状态
        if (task_scheduler.InFlightTasks().GetCount() > 0)
        {
            bool transfer_done = (!transfer_queue_ || transfer_queue_->IsLastSubmitComplete());
            bool graphics_done = (!graphics_queue_ || graphics_queue_->IsLastSubmitComplete());

            for (int i = task_scheduler.InFlightTasks().GetCount() - 1; i >= 0; --i)
            {
                GpuUploadTask *task = task_scheduler.InFlightTasks()[i];
                bool done = (task->backend == UploadBackendType::GpuTransferDma) ? transfer_done : graphics_done;
                if (done)
                {
                    if (task->staging_buffer)
                    {
                        task_scheduler.ReleaseStagingBytes(task->staging_bytes);
                        if (!task->is_ring_staging && task->owns_staging)
                            delete task->staging_buffer;
                        task->staging_buffer = nullptr;
                    }

                    if (task->is_ring_staging)
                    {
                        if (task->backend == UploadBackendType::GpuTransferDma && transfer_ring_)
                            transfer_ring_->Free(task->task_id);
                        else if (graphics_ring_)
                            graphics_ring_->Free(task->task_id);
                    }

                    if (task->state == UploadTaskState::InFlight)
                    {
                        task->state = UploadTaskState::Completed;
                        if (task->on_complete)
                            task->on_complete(task, task->user_data);
                        task_scheduler.PushCompleted(task);
                    }
                    else if (task->state == UploadTaskState::Discarded)
                    {
                        task_scheduler.PushCompleted(task);
                    }

                    task_scheduler.RemoveInFlight(i);
                }
            }
        }

        // 2. 按优先级由高到低调度就绪任务
        for (int p = 0; p < static_cast<int>(UploadPriority::Count); ++p)
        {
            bool stop_priority = false;

            while (task_scheduler.QueuedCount(size_t(p)) > 0)
            {
                GpuUploadTask *task = task_scheduler.QueuedHead(size_t(p));
                if (task->state == UploadTaskState::Cancelled)
                {
                    task_scheduler.Unregister(task);
                    delete task;
                    task_scheduler.PopQueued(size_t(p));
                    continue;
                }

                // Staging 显存背压控制
                if (task_scheduler.WouldExceedBudget(task))
                {
                    // 显存预算达上限，暂缓派发本批 DMA 任务
                    break;
                }

                task_scheduler.PopQueued(size_t(p));

                switch (DispatchTask(task))
                {
                    case DispatchOutcome::Submitted:
                        break;

                    case DispatchOutcome::Failed:
                        // 终态失败：退还 staging 记账、回调 on_complete、注销并销毁任务。
                        // 旧实现直接丢弃 ⇒ 上层永不知情且任务句柄悬空。
                        FailTask(task, u8"dispatch failed");
                        break;

                    case DispatchOutcome::Deferred:
                        // 队列暂时忙：放回队首，结束本优先级本轮调度（下轮 Update 再试）。
                        DeferTask(task);
                        stop_priority = true;
                        break;
                }

                if (stop_priority)
                    break;
            }
        }
    }

    void GpuUploadQueue::ExtractCompletedTasks(ValueArray<GpuUploadTask *> &out_completed)
    {
        MutexLock lock(&mutex_);
        task_scheduler.ExtractCompleted(out_completed);
    }
}
