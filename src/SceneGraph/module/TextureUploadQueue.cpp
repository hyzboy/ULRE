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

    TextureUploadQueue::TextureUploadQueue(VulkanDevice *device)
        : device_(device)
    {
        EnsureResources();
    }

    TextureUploadQueue::~TextureUploadQueue()
    {
        if (device_)
            device_->WaitIdle();

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

        GLogInfo(u8"[TextureUploadQueue] EnsureResources: Creating transfer_queue...");
        if (!transfer_queue_)
        {
            transfer_queue_ = device_->CreateTransferQueue("TextureUploadTransferQueue", 2);
        }
        GLogInfo(u8"[TextureUploadQueue] EnsureResources: Creating transfer_cmd_buf...");
        if (!transfer_cmd_buf_)
        {
            transfer_cmd_buf_ = device_->CreateTransferTextureCommandBuffer("TextureUploadTransferCmdBuf");
        }
        GLogInfo(u8"[TextureUploadQueue] EnsureResources: Creating graphics_queue...");
        if (!graphics_queue_)
        {
            graphics_queue_ = device_->CreateQueue("TextureUploadGraphicsQueue", 2);
        }
        GLogInfo(u8"[TextureUploadQueue] EnsureResources: Creating graphics_cmd_buf...");
        if (!graphics_cmd_buf_)
        {
            graphics_cmd_buf_ = device_->CreateTextureCommandBuffer("TextureUploadGraphicsCmdBuf");
        }
        GLogInfo(u8"[TextureUploadQueue] EnsureResources: Done");
    }

    uint64_t TextureUploadQueue::Enqueue(TextureUploadTask *task)
    {
        if (!task)
            return 0;

        ThreadMutexLock lock(&mutex_);

        const uint64_t task_id = next_task_id_++;
        task->task_id = task_id;
        task->state = UploadTaskState::Queued;

        // 自动判定后端
        if (task->backend == UploadBackendType::Auto)
        {
            const VulkanPhyDevice *phy = device_ ? device_->GetPhyDevice() : nullptr;
            const VulkanDevAttr *attr = device_ ? device_->GetDevAttr() : nullptr;
            const VkFormat fmt = task->target_texture ? task->target_texture->GetFormat() : VK_FORMAT_UNDEFINED;

            if (attr && attr->copy_memory_to_image && attr->transition_image_layout &&
                phy && phy->SupportHostImageCopyFormat(fmt) && !task->auto_mipmaps)
            {
                task->backend = UploadBackendType::HostImageCopy;
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
                delete task->staging_buffer;
                task->staging_buffer = nullptr;
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
            case UploadBackendType::HostImageCopy:
                return DispatchHostImageCopy(task);
            case UploadBackendType::GpuTransferDma:
                return DispatchGpuTransferDma(task);
            case UploadBackendType::GpuGraphicsDma:
            default:
                return DispatchGpuGraphicsDma(task);
        }
    }

    bool TextureUploadQueue::DispatchHostImageCopy(TextureUploadTask *task)
    {
        if (!task || !task->tci || !task->target_texture || !device_)
            return false;

        TextureCreateInfo *tci = task->tci;
        Texture *tex = task->target_texture;
        VkImage image = tex->GetImage();
        if (image == VK_NULL_HANDLE)
            return false;

        VulkanDevAttr *attr = device_->GetDevAttr();
        if (!attr || !attr->copy_memory_to_image || !attr->transition_image_layout)
            return false;

        const void *pixels = tci->pixels ? tci->pixels : (tci->buffer ? tci->buffer->Map() : nullptr);
        if (!pixels)
        {
            LogError(u8"[TextureUploadQueue] HostImageCopy failed: no pixel data");
            return false;
        }

        const uint32_t mip_levels = tci->target_mipmaps > 0 ? tci->target_mipmaps : 1;

        // 1. Transition layout: UNDEFINED -> GENERAL (Host Image Copy 规范通用目标布局)
        VkHostImageLayoutTransitionInfoEXT trans_info{};
        trans_info.sType = VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT;
        trans_info.pNext = nullptr;
        trans_info.image = image;
        trans_info.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        trans_info.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        trans_info.subresourceRange.aspectMask = tex->GetAspect();
        trans_info.subresourceRange.baseMipLevel = 0;
        trans_info.subresourceRange.levelCount = mip_levels;
        trans_info.subresourceRange.baseArrayLayer = 0;
        trans_info.subresourceRange.layerCount = 1;

        VkResult res = attr->transition_image_layout(device_->GetDevice(), 1, &trans_info);
        if (res != VK_SUCCESS)
        {
            LogError(u8"[TextureUploadQueue] Host transition UNDEFINED -> GENERAL failed: %d", res);
            if (tci->buffer) tci->buffer->Unmap();
            return false;
        }

        // 2. Prepare regions for each mip level
        AutoDeleteArray<VkMemoryToImageCopyEXT> regions(mip_levels);
        regions.zero();

        VkDeviceSize offset = 0;
        uint32_t width = tci->extent.width;
        uint32_t height = tci->extent.height;
        uint32_t rolling_level_bytes = tci->mipmap_zero_total_bytes;

        for (uint32_t level = 0; level < mip_levels; ++level)
        {
            VkMemoryToImageCopyEXT &reg = regions[level];
            reg.sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT;
            reg.pNext = nullptr;
            reg.pHostPointer = static_cast<const uint8_t *>(pixels) + offset;
            reg.memoryRowLength = 0;
            reg.memoryImageHeight = 0;
            reg.imageSubresource.aspectMask = tex->GetAspect();
            reg.imageSubresource.mipLevel = level;
            reg.imageSubresource.baseArrayLayer = 0;
            reg.imageSubresource.layerCount = 1;
            reg.imageOffset = {0, 0, 0};
            reg.imageExtent.width = width;
            reg.imageExtent.height = height;
            reg.imageExtent.depth = 1;

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

        VkCopyMemoryToImageInfoEXT copy_info{};
        copy_info.sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT;
        copy_info.pNext = nullptr;
        copy_info.flags = 0;
        copy_info.dstImage = image;
        copy_info.dstImageLayout = VK_IMAGE_LAYOUT_GENERAL;
        copy_info.regionCount = mip_levels;
        copy_info.pRegions = regions.data();

        res = attr->copy_memory_to_image(device_->GetDevice(), &copy_info);
        if (res != VK_SUCCESS)
        {
            LogError(u8"[TextureUploadQueue] vkCopyMemoryToImageEXT failed: %d", res);
            if (tci->buffer) tci->buffer->Unmap();
            return false;
        }

        // Host Image Copy 直接写入 GENERAL 布局，GPU 着色器通过 bindless 描述符直接以此布局采样
        tex->SetImageLayout(VK_IMAGE_LAYOUT_GENERAL);

        if (tci->buffer)
            tci->buffer->Unmap();

        task->state = UploadTaskState::Completed;
        if (task->on_complete)
            task->on_complete(task, task->user_data);

        completed_tasks_.Add(task);

        LogInfo(u8"[TextureUploadQueue] HostImageCopy completed for task %llu (tex=%p)",
                task->task_id, (const void *)tex);
        return true;
    }

    bool TextureUploadQueue::DispatchGpuTransferDma(TextureUploadTask *task)
    {
        if (!task || !task->tci || !task->target_texture || !device_)
            return false;

        EnsureResources();

        TextureCreateInfo *tci = task->tci;
        Texture *tex = task->target_texture;
        VkImage image = tex->GetImage();
        if (image == VK_NULL_HANDLE)
            return false;

        const void *pixels = tci->pixels ? tci->pixels : (tci->buffer ? tci->buffer->Map() : nullptr);
        const VkDeviceSize total_bytes = tci->total_bytes > 0 ? tci->total_bytes : (tci->buffer ? tci->buffer->GetSize() : 0);
        if (!pixels || total_bytes == 0)
        {
            LogError(u8"[TextureUploadQueue] GpuTransferDma failed: no pixel data");
            return false;
        }

        task->staging_bytes = total_bytes;
        task->staging_buffer = device_->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, total_bytes,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (!task->staging_buffer)
        {
            if (tci->buffer) tci->buffer->Unmap();
            return false;
        }

        void *dst = task->staging_buffer->Map();
        if (dst)
        {
            memcpy(dst, pixels, total_bytes);
            task->staging_buffer->Unmap();
        }
        if (tci->buffer)
            tci->buffer->Unmap();

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
            bic.bufferOffset = offset;
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
            VK_ACCESS_2_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            subresource_range);

        transfer_cmd_buf_->End();

        Semaphore *sem = device_->CreateGPUSemaphore("TextureUploadTransferCompleteSem");
        task->transfer_sem = sem;
        task->signal_semaphore = *sem;

        transfer_queue_->Submit(transfer_cmd_buf_, nullptr, sem);

        pending_wait_semaphores_.Add(*sem);
        task->state = UploadTaskState::InFlight;
        in_flight_tasks_.Add(task);

        if (task->priority == UploadPriority::Immediate)
        {
            transfer_queue_->WaitLastSubmitFence();
            current_staging_bytes_ -= task->staging_bytes;
            delete task->staging_buffer;
            task->staging_buffer = nullptr;

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

        TextureCreateInfo *tci = task->tci;
        Texture *tex = task->target_texture;
        VkImage image = tex->GetImage();
        if (image == VK_NULL_HANDLE)
            return false;

        const void *pixels = tci->pixels ? tci->pixels : (tci->buffer ? tci->buffer->Map() : nullptr);
        const VkDeviceSize total_bytes = tci->total_bytes > 0 ? tci->total_bytes : (tci->buffer ? tci->buffer->GetSize() : 0);
        if (!pixels || total_bytes == 0)
        {
            LogError(u8"[TextureUploadQueue] GpuGraphicsDma failed: no pixel data");
            return false;
        }

        task->staging_bytes = total_bytes;
        task->staging_buffer = device_->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, total_bytes,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (!task->staging_buffer)
        {
            if (tci->buffer) tci->buffer->Unmap();
            return false;
        }

        void *dst = task->staging_buffer->Map();
        if (dst)
        {
            memcpy(dst, pixels, total_bytes);
            task->staging_buffer->Unmap();
        }
        if (tci->buffer)
            tci->buffer->Unmap();

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
            bic.bufferOffset = 0;
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
                bic.bufferOffset = offset;
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

        graphics_queue_->Submit(graphics_cmd_buf_, nullptr, nullptr);

        task->state = UploadTaskState::InFlight;
        in_flight_tasks_.Add(task);

        if (task->priority == UploadPriority::Immediate)
        {
            graphics_queue_->WaitLastSubmitFence();
            current_staging_bytes_ -= task->staging_bytes;
            delete task->staging_buffer;
            task->staging_buffer = nullptr;

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
                        delete task->staging_buffer;
                        task->staging_buffer = nullptr;
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
                if (task->backend != UploadBackendType::HostImageCopy)
                {
                    if (current_staging_bytes_ + task->staging_bytes > max_staging_bytes_ && current_staging_bytes_ > 0)
                    {
                        // 显存预算达上限，暂缓派发本批 DMA 任务
                        break;
                    }
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
