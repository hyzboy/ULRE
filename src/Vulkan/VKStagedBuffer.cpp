#include<hgl/vk/VKStagedBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKMemory.h>
#include<hgl/log/Log.h>
#include<string.h>

namespace hgl::graph{

StagedBuffer::StagedBuffer(VkDevice dev,
                           VkBuffer staging_buf, DeviceMemory *staging_mem,
                           VkBuffer device_buf, DeviceMemory *device_mem,
                           VkDeviceSize size, VkBufferUsageFlags usage_flags)
{
    device = dev;
    staging_buffer = staging_buf;
    staging_memory = staging_mem;
    device_buffer = device_buf;
    device_memory = device_mem;
    buffer_size = size;
    usage = usage_flags;
    is_dirty = false;
    dirty_offset = 0;
    dirty_size = 0;

    // Register with the device's IGPUBuffer registry
    VulkanDevice *owner = VulkanDevice::FromDevice(device);
    if (owner)
        owner->RegisterGPUBuffer(this);
}

StagedBuffer::~StagedBuffer()
{
    VulkanDevice *owner = VulkanDevice::FromDevice(device);
    if (owner)
    {
        owner->UnregisterGPUBuffer(this);
        if (staging_buffer)
            owner->UntrackObject(VK_OBJECT_TYPE_BUFFER, (uint64_t)(uintptr_t)staging_buffer);
        if (staging_memory)
            owner->UntrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)(uintptr_t)static_cast<VkDeviceMemory>(*staging_memory));
    }

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

    // Mark as dirty (no queue notification needed — ECS System polls IsDirty)
    MarkDirty(offset, size);

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

}//namespace hgl::graph
