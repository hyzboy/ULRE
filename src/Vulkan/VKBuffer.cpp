#include<hgl/vk/VKBuffer.h>

namespace hgl::graph
{
void VKDescriptorBuffer::FlushRanges(const IGPUBuffer::DirtyRange *ranges, size_t count)
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
}//namespace hgl::graph
