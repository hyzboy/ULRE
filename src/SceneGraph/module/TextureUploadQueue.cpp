#include <hgl/graph/module/TextureUploadQueue.h>
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

    TextureUploadQueue::TextureUploadQueue(VulkanDevice *device)
        : device_(device)
    {
        EnsureResources();
    }

    TextureUploadQueue::~TextureUploadQueue()
    {
        if (device_)
            device_->WaitIdle();

        SAFE_CLEAR(transfer_ring_);
        SAFE_CLEAR(graphics_ring_);
        SAFE_CLEAR(transfer_cmd_buf_);
        SAFE_CLEAR(graphics_cmd_buf_);
        SAFE_CLEAR(transfer_queue_);
        SAFE_CLEAR(graphics_queue_);

        for (int p = 0; p < static_cast<int>(UploadPriority::Count); ++p)
        {
            for (int i = 0; i < queued_tasks_[p].GetCount(); ++i)
            {
                delete queued_tasks_[p][i];
            }
            queued_tasks_[p].Clear();
        }

        for (int i = 0; i < in_flight_tasks_.GetCount(); ++i)
        {
            delete in_flight_tasks_[i];
        }
        in_flight_tasks_.Clear();

        for (int i = 0; i < completed_tasks_.GetCount(); ++i)
        {
            delete completed_tasks_[i];
        }
        completed_tasks_.Clear();

        all_tasks_.Clear();
        pending_wait_semaphores_.Clear();
    }

    void TextureUploadQueue::EnsureResources()
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
            transfer_queue_ = device_->CreateTransferQueue("TextureUploadTransferQueue", 2);
        }
        if (!transfer_cmd_buf_)
        {
            transfer_cmd_buf_ = device_->CreateTransferTextureCommandBuffer("TextureUploadTransferCmdBuf");
        }
        if (!graphics_queue_)
        {
            graphics_queue_ = device_->CreateQueue("TextureUploadGraphicsQueue", 2);
        }
        if (!graphics_cmd_buf_)
        {
            graphics_cmd_buf_ = device_->CreateTextureCommandBuffer("TextureUploadGraphicsCmdBuf");
        }
    }

    uint64_t TextureUploadQueue::Enqueue(TextureUploadTask *task)
    {
        if (!task)
            return 0;

        ThreadMutexLock lock(&mutex_);

        const uint64_t task_id = next_task_id_++;
        task->task_id = task_id;
        task->state = UploadTaskState::Queued;

        // 自动判定后端（仅在 GPU 专用 Transfer DMA 与 Graphics DMA 之间选择）
        if (task->backend == UploadBackendType::Auto)
        {
            if (device_ && device_->GetTransferFamilyIndex() != VK_QUEUE_FAMILY_IGNORED &&
                !task->auto_mipmaps && task->priority != UploadPriority::Immediate)
            {
                task->backend = UploadBackendType::GpuTransferDma;
            }
            else
            {
                task->backend = UploadBackendType::GpuGraphicsDma;
            }
        }

        all_tasks_.Add(task_id, task);

        // Immediate 任务直接同步派发并等待完成
        if (task->priority == UploadPriority::Immediate)
        {
            DispatchTask(task);
            return task_id;
        }

        queued_tasks_[static_cast<size_t>(task->priority)].Add(task);
        return task_id;
    }

    bool TextureUploadQueue::CancelUpload(uint64_t task_id)
    {
        ThreadMutexLock lock(&mutex_);

        TextureUploadTask **task_ptr = all_tasks_.GetValuePointer(task_id);
        if (!task_ptr || !*task_ptr)
            return false;

        TextureUploadTask *task = *task_ptr;
        if (task->state == UploadTaskState::Queued)
        {
            task->state = UploadTaskState::Cancelled;
            return true;
        }
        else if (task->state == UploadTaskState::InFlight)
        {
            task->state = UploadTaskState::Discarded;
            return true;
        }

        return false;
    }

    UploadTaskState TextureUploadQueue::GetTaskState(uint64_t task_id)
    {
        ThreadMutexLock lock(&mutex_);
        TextureUploadTask **task_ptr = all_tasks_.GetValuePointer(task_id);
        if (task_ptr && *task_ptr)
            return (*task_ptr)->state;

        if (completed_task_ids_.Contains(task_id))
            return UploadTaskState::Completed;

        return UploadTaskState::Cancelled;
    }

    TextureUploadTask *TextureUploadQueue::FindTask(uint64_t task_id)
    {
        ThreadMutexLock lock(&mutex_);
        TextureUploadTask **task_ptr = all_tasks_.GetValuePointer(task_id);
        return task_ptr ? *task_ptr : nullptr;
    }

    void TextureUploadQueue::WaitTask(uint64_t task_id)
    {
        TextureUploadTask *task = FindTask(task_id);
        if (!task)
            return;

        if (task->state == UploadTaskState::Queued)
        {
            ThreadMutexLock lock(&mutex_);
            for (int p = 0; p < static_cast<int>(UploadPriority::Count); ++p)
            {
                int idx = queued_tasks_[p].Find(task);
                if (idx != -1)
                {
                    queued_tasks_[p].Delete(idx);
                    break;
                }
            }
            DispatchTask(task);
        }

        if (task->state == UploadTaskState::InFlight)
        {
            if (task->backend == UploadBackendType::GpuTransferDma && transfer_queue_)
            {
                transfer_queue_->WaitLastSubmitFence();
            }
            else if (graphics_queue_)
            {
                graphics_queue_->WaitLastSubmitFence();
            }

            if (task->staging_buffer)
            {
                current_staging_bytes_ -= task->staging_bytes;
                if (!task->is_ring_staging)
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

    bool TextureUploadQueue::DispatchTask(TextureUploadTask *task)
    {
        if (!task)
            return false;

        switch (task->backend)
        {
            case UploadBackendType::GpuTransferDma:
                return DispatchGpuTransferDma(task);
            case UploadBackendType::GpuGraphicsDma:
            default:
                return DispatchGpuGraphicsDma(task);
        }
    }

    bool TextureUploadQueue::DispatchGpuTransferDma(TextureUploadTask *task)
    {
        if (!task || !task->tci || !task->target_texture || !device_)
            return false;

        EnsureResources();

        if (transfer_queue_)
            transfer_queue_->WaitLastSubmitFence();

        TextureCreateInfo *tci = task->tci;
        Texture *tex = task->target_texture;
        VkImage image = tex->GetImage();
        if (image == VK_NULL_HANDLE)
            return false;

        const VkDeviceSize total_bytes = tci->total_bytes > 0 ? tci->total_bytes : (tci->buffer ? tci->buffer->GetSize() : 0);
        if (total_bytes == 0)
        {
            LogError(u8"[TextureUploadQueue] GpuTransferDma failed: zero total bytes");
            return false;
        }

        task->staging_bytes = total_bytes;

        if (tci->buffer)
        {
            // 零拷贝直通：直接接管 tci->buffer 作为 Transfer 源缓冲，消除额外的 CPU 内存分配与 memcpy
            task->staging_buffer = tci->buffer;
            tci->buffer = nullptr;
            task->is_ring_staging = false;
            task->ring_offset = 0;
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
                    return false;

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
            LogError(u8"[TextureUploadQueue] GpuTransferDma failed: no pixel data or buffer");
            return false;
        }

        current_staging_bytes_ += task->staging_bytes;

        const uint32_t mip_levels = tci->target_mipmaps > 0 ? tci->target_mipmaps : 1;

        transfer_cmd_buf_->Begin();

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

        transfer_cmd_buf_->End();

        tex->SetImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        Semaphore *sem = device_->CreateGPUSemaphore("TextureUploadTransferCompleteSem");
        task->transfer_sem = sem;
        task->signal_semaphore = *sem;

        SemaphoreSubmit transfer_signal = SemaphoreSubmit::Signal(sem);
        transfer_queue_->Submit(transfer_cmd_buf_, nullptr, 0, &transfer_signal, 1);

        pending_wait_semaphores_.Add(sem);
        task->state = UploadTaskState::InFlight;
        in_flight_tasks_.Add(task);

        if (task->priority == UploadPriority::Immediate)
        {
            transfer_queue_->WaitLastSubmitFence();
            current_staging_bytes_ -= task->staging_bytes;
            if (!task->is_ring_staging && task->staging_buffer)
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

            in_flight_tasks_.DeleteByValue(task);
            completed_tasks_.Add(task);
        }

        return true;
    }

    bool TextureUploadQueue::DispatchGpuGraphicsDma(TextureUploadTask *task)
    {
        if (!task || !task->tci || !task->target_texture || !device_)
            return false;

        EnsureResources();

        if (graphics_queue_)
            graphics_queue_->WaitLastSubmitFence();

        TextureCreateInfo *tci = task->tci;
        Texture *tex = task->target_texture;
        VkImage image = tex->GetImage();
        if (image == VK_NULL_HANDLE)
            return false;

        const VkDeviceSize total_bytes = tci->total_bytes > 0 ? tci->total_bytes : (tci->buffer ? tci->buffer->GetSize() : 0);
        if (total_bytes == 0)
        {
            LogError(u8"[TextureUploadQueue] GpuGraphicsDma failed: zero total bytes");
            return false;
        }

        task->staging_bytes = total_bytes;

        if (tci->buffer)
        {
            // 零拷贝直通：直接接管 tci->buffer 作为 Transfer 源缓冲，消除额外的 CPU 内存分配与 memcpy
            task->staging_buffer = tci->buffer;
            tci->buffer = nullptr;
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
                    return false;

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
            LogError(u8"[TextureUploadQueue] GpuGraphicsDma failed: no pixel data or buffer");
            return false;
        }

        current_staging_bytes_ += task->staging_bytes;

        const uint32_t mip_levels = tci->target_mipmaps > 0 ? tci->target_mipmaps : 1;

        graphics_cmd_buf_->Begin();

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

        graphics_cmd_buf_->ImageMemoryBarrier2(
            image,
            VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT,
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            subresource_range);

        graphics_cmd_buf_->End();

        tex->SetImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        graphics_queue_->Submit(graphics_cmd_buf_, nullptr, 0, nullptr, 0);

        task->state = UploadTaskState::InFlight;
        in_flight_tasks_.Add(task);

        if (task->priority == UploadPriority::Immediate)
        {
            graphics_queue_->WaitLastSubmitFence();
            current_staging_bytes_ -= task->staging_bytes;
            if (!task->is_ring_staging && task->staging_buffer)
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

            in_flight_tasks_.DeleteByValue(task);
            completed_tasks_.Add(task);
        }

        return true;
    }

    void TextureUploadQueue::Update()
    {
        ThreadMutexLock lock(&mutex_);

        // 1. 检查已在飞任务的完成状态
        if (in_flight_tasks_.GetCount() > 0)
        {
            bool transfer_done = (!transfer_queue_ || transfer_queue_->IsLastSubmitComplete());
            bool graphics_done = (!graphics_queue_ || graphics_queue_->IsLastSubmitComplete());

            for (int i = in_flight_tasks_.GetCount() - 1; i >= 0; --i)
            {
                TextureUploadTask *task = in_flight_tasks_[i];
                bool done = (task->backend == UploadBackendType::GpuTransferDma) ? transfer_done : graphics_done;
                if (done)
                {
                    if (task->staging_buffer)
                    {
                        current_staging_bytes_ -= task->staging_bytes;
                        if (!task->is_ring_staging)
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
                        completed_tasks_.Add(task);
                    }
                    else if (task->state == UploadTaskState::Discarded)
                    {
                        completed_tasks_.Add(task);
                    }

                    in_flight_tasks_.Delete(i);
                }
            }
        }

        // 2. 按优先级由高到低调度就绪任务
        for (int p = 0; p < static_cast<int>(UploadPriority::Count); ++p)
        {
            while (queued_tasks_[p].GetCount() > 0)
            {
                TextureUploadTask *task = queued_tasks_[p][0];
                if (task->state == UploadTaskState::Cancelled)
                {
                    all_tasks_.DeleteByKey(task->task_id);
                    delete task;
                    queued_tasks_[p].Delete(0);
                    continue;
                }

                // Staging 显存背压控制
                if (current_staging_bytes_ + task->staging_bytes > max_staging_bytes_ && current_staging_bytes_ > 0)
                {
                    // 显存预算达上限，暂缓派发本批 DMA 任务
                    break;
                }

                queued_tasks_[p].Delete(0);
                DispatchTask(task);
            }
        }
    }

    void TextureUploadQueue::ExtractCompletedTasks(ValueArray<TextureUploadTask *> &out_completed)
    {
        ThreadMutexLock lock(&mutex_);
        out_completed.Clear();
        for (int i = 0; i < completed_tasks_.GetCount(); ++i)
        {
            TextureUploadTask *task = completed_tasks_[i];
            out_completed.Add(task);
            completed_task_ids_.Add(task->task_id);
            all_tasks_.DeleteByKey(task->task_id);
        }
        completed_tasks_.Clear();
    }
}
