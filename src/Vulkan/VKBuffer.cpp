#include<hgl/vk/VKBuffer.h>

namespace hgl::graph
{
void *DeviceBuffer::Map(VkDeviceSize start, VkDeviceSize size)
{
    if(staged_source)
        return staged_source->Map(start, size);

    return buf.memory ? buf.memory->Map(start, size) : nullptr;
}

void DeviceBuffer::Flush(VkDeviceSize start, VkDeviceSize size)
{
    if(staged_source)
    {
        staged_source->MarkDirty(start, size);
        return;
    }

    if(buf.memory)
        buf.memory->Flush(start, size);
}

void DeviceBuffer::Flush(VkDeviceSize size)
{
    if(staged_source)
    {
        staged_source->MarkDirty(0, size);
        return;
    }

    if(buf.memory)
        buf.memory->Flush(size);
}

void DeviceBuffer::FlushRanges(const IGPUBuffer::DirtyRange *ranges, size_t count)
{
    if(!ranges || count == 0)
        return;

    if(staged_source)
    {
        staged_source->MarkDirtyRanges(ranges, count);
        return;
    }

    if(!buf.memory)
        return;

    const VkDeviceSize total_size = GetSize();
    for(size_t i = 0; i < count; ++i)
    {
        VkDeviceSize offset = ranges[i].offset;
        VkDeviceSize size   = ranges[i].size;

        if(offset >= total_size)
            continue;

        if(size == VK_WHOLE_SIZE || offset + size > total_size)
            size = total_size - offset;

        if(size == 0)
            continue;

        buf.memory->Flush(offset, size);
    }
}

bool DeviceBuffer::Write(const void *ptr, uint32_t start, uint32_t size)
{
    if(staged_source)
        return staged_source->Write(ptr, (VkDeviceSize)start, (VkDeviceSize)size);

    return buf.memory ? buf.memory->Write(ptr, start, size) : false;
}

bool DeviceBuffer::Write(const void *ptr, uint32_t size)
{
    if(staged_source)
        return staged_source->Write(ptr, 0, (VkDeviceSize)size);

    return buf.memory ? buf.memory->Write(ptr, 0, size) : false;
}

}//namespace hgl::graph
