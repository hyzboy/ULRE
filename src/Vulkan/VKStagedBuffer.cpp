#include<hgl/vk/VKStagedBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKMemory.h>
#include<hgl/log/Log.h>
#include<string.h>

namespace hgl::graph{

StagedBuffer::StagedBuffer(const std::string &name,
                           VkDevice dev,
                           VkBuffer staging_buf, DeviceMemory *staging_mem,
                           VkBuffer device_buf, DeviceMemory *device_mem,
                           VkDeviceSize size, VkBufferUsageFlags usage_flags)
    : IGPUBuffer(name)
{
    device = dev;
    staging_buffer = staging_buf;
    staging_memory = staging_mem;
    device_buffer = device_buf;
    device_memory = device_mem;
    buffer_size = size;
    usage = usage_flags;

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

    mapped_offset = 0;
    mapped_size   = buffer_size;
    return staging_memory->Map();
}

void * StagedBuffer::Map(VkDeviceSize offset, VkDeviceSize size)
{
    if (!staging_memory)
        return nullptr;

    mapped_offset = offset;
    mapped_size   = size;
    return staging_memory->Map(offset, size);
}

void StagedBuffer::Unmap()
{
    if (staging_memory)
    {
        staging_memory->Unmap();
        MarkDirty(mapped_offset, mapped_size);
        mapped_offset = 0;
        mapped_size   = 0;
    }
}

void StagedBuffer::MarkDirty(VkDeviceSize offset, VkDeviceSize size)
{
    TrackDirtyRange(buffer_size, offset, size);
}

void StagedBuffer::MarkDirtyRanges(const DirtyRange *ranges, size_t count)
{
    TrackDirtyRanges(buffer_size, ranges, count);
}

void StagedBuffer::CopyToDevice(VkCommandBuffer cmd)
{
    if (!cmd || !HasTrackedDirty())
        return;

    const auto &dirty_ranges = GetTrackedDirtyRanges();

    if (!dirty_ranges.empty())
    {
        std::vector<VkBufferCopy> copy_regions;
        copy_regions.reserve(dirty_ranges.size());

        for (const auto &range : dirty_ranges)
        {
            VkBufferCopy copy_region = {};
            copy_region.srcOffset = range.offset;
            copy_region.dstOffset = range.offset;
            copy_region.size = range.size;
            copy_regions.push_back(copy_region);
        }

        vkCmdCopyBuffer(cmd, staging_buffer, device_buffer,
                        static_cast<uint32_t>(copy_regions.size()),
                        copy_regions.data());
    }
    else if (GetTrackedDirtySize() > 0)
    {
        VkBufferCopy copy_region = {};
        copy_region.srcOffset = GetTrackedDirtyOffset();
        copy_region.dstOffset = GetTrackedDirtyOffset();
        copy_region.size = GetTrackedDirtySize();

        vkCmdCopyBuffer(cmd, staging_buffer, device_buffer, 1, &copy_region);
    }

    ClearDirty();

    //GLogInfo("[StagedBuffer] CopyToDevice buffer=%p offset=%llu size=%llu",
    //         (void *)this,
    //         static_cast<unsigned long long>(copy_region.srcOffset),
    //         static_cast<unsigned long long>(copy_region.size));
}

void StagedBuffer::ClearDirty()
{
    ClearTrackedDirty();
}

}//namespace hgl::graph
