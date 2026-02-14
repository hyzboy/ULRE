#include<hgl/vk/VKStagedBuffer.h>
#include<hgl/vk/VKBufferUpdateQueue.h>
#include<hgl/vk/VKMemory.h>
#include<hgl/log/Log.h>
#include<string.h>

VK_NAMESPACE_BEGIN

StagedBuffer::StagedBuffer(VkDevice dev, BufferUpdateQueue *queue,
                           VkBuffer staging_buf, DeviceMemory *staging_mem,
                           VkBuffer device_buf, DeviceMemory *device_mem,
                           VkDeviceSize size, VkBufferUsageFlags usage_flags)
{
    device = dev;
    update_queue = queue;
    staging_buffer = staging_buf;
    staging_memory = staging_mem;
    device_buffer = device_buf;
    device_memory = device_mem;
    buffer_size = size;
    usage = usage_flags;
    is_dirty = false;
    dirty_offset = 0;
    dirty_size = 0;
}

StagedBuffer::~StagedBuffer()
{
    if (staging_buffer)
        vkDestroyBuffer(device, staging_buffer, nullptr);

    if (device_buffer)
        vkDestroyBuffer(device, device_buffer, nullptr);

    delete staging_memory;
    delete device_memory;
}

bool StagedBuffer::Write(const void *data, VkDeviceSize offset, VkDeviceSize size)
{
    if (!data || !staging_memory)
        return false;

    if (offset + size > buffer_size)
        return false;

    // Write to staging buffer
    if (!staging_memory->Write(data, offset, size))
        return false;

    // Mark as dirty and add to update queue
    MarkDirty(offset, size);

    //GLogInfo("[StagedBuffer] Write buffer=%p offset=%llu size=%llu",
    //         (void *)this,
    //         static_cast<unsigned long long>(offset),
    //         static_cast<unsigned long long>(size));

    return true;
}

void * StagedBuffer::Map()
{
    if (!staging_memory)
        return nullptr;

    return staging_memory->Map();
}

void * StagedBuffer::Map(VkDeviceSize offset, VkDeviceSize size)
{
    if (!staging_memory)
        return nullptr;

    return staging_memory->Map(offset, size);
}

void StagedBuffer::Unmap()
{
    if (staging_memory)
        staging_memory->Unmap();
}

void StagedBuffer::MarkDirty(VkDeviceSize offset, VkDeviceSize size)
{
    if (size == VK_WHOLE_SIZE)
        size = buffer_size - offset;

    if (!is_dirty)
    {
        // First time marking dirty
        is_dirty = true;
        dirty_offset = offset;
        dirty_size = size;
    }
    else
    {
        // Merge dirty regions
        VkDeviceSize end1 = dirty_offset + dirty_size;
        VkDeviceSize end2 = offset + size;

        dirty_offset = hgl_min(dirty_offset, offset);
        dirty_size = hgl_max(end1, end2) - dirty_offset;
    }

    // Add to update queue
    if (update_queue)
        update_queue->AddUpdate(this, dirty_offset, dirty_size);

    //GLogInfo("[StagedBuffer] MarkDirty buffer=%p offset=%llu size=%llu",
    //         (void *)this,
    //         static_cast<unsigned long long>(dirty_offset),
    //         static_cast<unsigned long long>(dirty_size));
}

void StagedBuffer::CopyToDevice(VkCommandBuffer cmd)
{
    if (!cmd || !is_dirty)
        return;

    VkBufferCopy copy_region = {};
    copy_region.srcOffset = dirty_offset;
    copy_region.dstOffset = dirty_offset;
    copy_region.size = dirty_size;

    vkCmdCopyBuffer(cmd, staging_buffer, device_buffer, 1, &copy_region);

    ClearDirty();

    //GLogInfo("[StagedBuffer] CopyToDevice buffer=%p offset=%llu size=%llu",
    //         (void *)this,
    //         static_cast<unsigned long long>(copy_region.srcOffset),
    //         static_cast<unsigned long long>(copy_region.size));
}

void StagedBuffer::ClearDirty()
{
    is_dirty = false;
    dirty_offset = 0;
    dirty_size = 0;
}

VK_NAMESPACE_END
