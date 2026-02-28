#include<hgl/vk/VKStagedBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKMemory.h>
#include<hgl/log/Log.h>
#include<algorithm>
#include<string.h>

namespace hgl::graph{

namespace
{
    static bool NormalizeRange(VkDeviceSize buffer_size, VkDeviceSize &offset, VkDeviceSize &size)
    {
        if (offset >= buffer_size)
            return false;

        if (size == VK_WHOLE_SIZE || offset + size > buffer_size)
            size = buffer_size - offset;

        return size > 0;
    }

    static void MergeDirtyRanges(std::vector<IGPUBuffer::DirtyRange> &ranges)
    {
        if (ranges.size() <= 1)
            return;

        std::sort(ranges.begin(), ranges.end(), [](const IGPUBuffer::DirtyRange &a, const IGPUBuffer::DirtyRange &b)
        {
            return a.offset < b.offset;
        });

        size_t write_index = 0;
        for (size_t read_index = 1; read_index < ranges.size(); ++read_index)
        {
            auto &last = ranges[write_index];
            const auto &cur = ranges[read_index];

            const VkDeviceSize last_end = last.offset + last.size;
            const VkDeviceSize cur_end = cur.offset + cur.size;

            if (cur.offset <= last_end)
            {
                if (cur_end > last_end)
                    last.size = cur_end - last.offset;
            }
            else
            {
                ++write_index;
                ranges[write_index] = cur;
            }
        }

        ranges.resize(write_index + 1);
    }
}

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
    if (!NormalizeRange(buffer_size, offset, size))
        return;

    if (!is_dirty)
    {
        is_dirty = true;
        dirty_offset = offset;
        dirty_size = size;
    }
    else
    {
        VkDeviceSize end1 = dirty_offset + dirty_size;
        VkDeviceSize end2 = offset + size;

        dirty_offset = hgl_min(dirty_offset, offset);
        dirty_size = hgl_max(end1, end2) - dirty_offset;
    }

    dirty_ranges.push_back({offset, size});
    MergeDirtyRanges(dirty_ranges);
}

void StagedBuffer::MarkDirtyRanges(const DirtyRange *ranges, size_t count)
{
    if (!ranges || count == 0)
        return;

    bool any_dirty = false;
    for (size_t i = 0; i < count; ++i)
    {
        VkDeviceSize offset = ranges[i].offset;
        VkDeviceSize size = ranges[i].size;

        if (!NormalizeRange(buffer_size, offset, size))
            continue;

        any_dirty = true;
        dirty_ranges.push_back({offset, size});

        if (!is_dirty)
        {
            is_dirty = true;
            dirty_offset = offset;
            dirty_size = size;
        }
        else
        {
            VkDeviceSize end1 = dirty_offset + dirty_size;
            VkDeviceSize end2 = offset + size;

            dirty_offset = hgl_min(dirty_offset, offset);
            dirty_size = hgl_max(end1, end2) - dirty_offset;
        }
    }

    if (any_dirty)
        MergeDirtyRanges(dirty_ranges);
}

void StagedBuffer::CopyToDevice(VkCommandBuffer cmd)
{
    if (!cmd || !is_dirty)
        return;

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
    else if (dirty_size > 0)
    {
        VkBufferCopy copy_region = {};
        copy_region.srcOffset = dirty_offset;
        copy_region.dstOffset = dirty_offset;
        copy_region.size = dirty_size;

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
    is_dirty = false;
    dirty_offset = 0;
    dirty_size = 0;
    dirty_ranges.clear();
}

}//namespace hgl::graph
