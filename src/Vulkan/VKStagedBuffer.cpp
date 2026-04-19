#include<hgl/vk/VKStagedBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/log/Log.h>
#include<string.h>

namespace hgl::graph{

StagedBuffer::StagedBuffer(const std::string &name,
                           VkDevice dev,
                           VmaAllocator alloc,
                           VkBuffer staging_buf,
                           VmaAllocation staging_alloc,
                           VkDeviceMemory staging_mem,
                           VkBuffer device_buf,
                           VmaAllocation device_alloc,
                           VkDeviceMemory device_mem,
                           VkDeviceSize size,
                           VkBufferUsageFlags usage_flags)
    : IGPUBuffer(name)
{
    device = dev;
    allocator = alloc;
    staging_buffer = staging_buf;
    staging_allocation = staging_alloc;
    staging_vk_memory = staging_mem;
    device_buffer = device_buf;
    device_allocation = device_alloc;
    device_vk_memory = device_mem;
    buffer_size = size;
    usage = usage_flags;

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
        if (staging_vk_memory)
            owner->UntrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)(uintptr_t)staging_vk_memory);
    }

    if(mapped_ptr)
    {
        vmaUnmapMemory(allocator, staging_allocation);
        mapped_ptr = nullptr;
    }

    if (staging_buffer && staging_allocation)
        vmaDestroyBuffer(allocator, staging_buffer, staging_allocation);

    if (device_buffer && device_allocation)
        vmaDestroyBuffer(allocator, device_buffer, device_allocation);
}

bool StagedBuffer::Write(const void *data, VkDeviceSize offset, VkDeviceSize size)
{
    if (!data || !staging_allocation)
        return false;

    if (offset + size > buffer_size)
        return false;

    if(mapped_ptr)
    {
        memcpy(static_cast<char *>(mapped_ptr) + offset, data, static_cast<size_t>(size));
    }
    else
    {
        void *ptr = nullptr;
        if(vmaMapMemory(allocator, staging_allocation, &ptr) != VK_SUCCESS || !ptr)
            return false;

        memcpy(static_cast<char *>(ptr) + offset, data, static_cast<size_t>(size));
        vmaUnmapMemory(allocator, staging_allocation);
    }

    MarkDirty(offset, size);

    return true;
}

void * StagedBuffer::Map()
{
    mapped_offset = 0;
    mapped_size   = buffer_size;

    if(mapped_ptr)
        return mapped_ptr;

    if(vmaMapMemory(allocator, staging_allocation, &mapped_ptr) != VK_SUCCESS)
    {
        mapped_ptr = nullptr;
        return nullptr;
    }

    return mapped_ptr;
}

void * StagedBuffer::Map(VkDeviceSize offset, VkDeviceSize size)
{
    if(offset >= buffer_size)
        return nullptr;

    if(size == VK_WHOLE_SIZE || offset + size > buffer_size)
        size = buffer_size - offset;

    if(size == 0)
        return nullptr;

    mapped_offset = offset;
    mapped_size   = size;

    if(!mapped_ptr)
    {
        if(vmaMapMemory(allocator, staging_allocation, &mapped_ptr) != VK_SUCCESS)
        {
            mapped_ptr = nullptr;
            return nullptr;
        }
    }

    return static_cast<char *>(mapped_ptr) + offset;
}

void StagedBuffer::Unmap()
{
    if (mapped_ptr)
    {
        vmaUnmapMemory(allocator, staging_allocation);
        mapped_ptr = nullptr;
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
}

void StagedBuffer::ClearDirty()
{
    ClearTrackedDirty();
}

}//namespace hgl::graph
