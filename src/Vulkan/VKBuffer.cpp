#include<hgl/vk/VKBuffer.h>

namespace hgl::graph{

// Out-of-line destructor: provides the symbol expected by stale OBJ files compiled
// before VKBuffer.h switched to ~DeviceBuffer() = default.  Cleanup is in VkBufferOwner.
DeviceBuffer::~DeviceBuffer() {}

void *DeviceBuffer::Map()
{
    if(staged_source)
        return staged_source->Map(0, staged_source->GetSize());

    return buf.memory ? buf.memory->Map() : nullptr;
}

void *DeviceBuffer::Map(VkDeviceSize start, VkDeviceSize size)
{
    if(staged_source)
        return staged_source->Map(start, size);

    return buf.memory ? buf.memory->Map(start, size) : nullptr;
}

void DeviceBuffer::Unmap()
{
    if(staged_source)
    {
        staged_source->Unmap();
        return;
    }

    if(buf.memory)
        buf.memory->Unmap();
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

bool DeviceBuffer::Write(const void *ptr)
{
    if(staged_source)
        return staged_source->Write(ptr, 0, staged_source->GetSize());

    return buf.memory ? buf.memory->Write(ptr) : false;
}

}//namespace hgl::graph