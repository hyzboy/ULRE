#include<hgl/vk/VKRingBufferWrapper.h>
#include<hgl/vk/VKBuffer.h>

namespace hgl::graph
{
void* RingBufferWrapper::MapRange(VkDeviceSize offset, VkDeviceSize count)
{
    // For ring buffer, offset and count are treated as static_count and dynamic_count
    // This is a simplified adaptation; real use should call MapDynamicRange directly
    void *ptr = MapDynamicRange(0, static_cast<uint32_t>(count));
    dirty = (ptr != nullptr);
    return ptr;
}

bool RingBufferWrapper::WriteRange(const void *data, VkDeviceSize offset, VkDeviceSize count)
{
    // For ring buffer, offset is static_count, count is dynamic_count
    return WriteDynamicRange(data, static_cast<uint32_t>(offset), static_cast<uint32_t>(count));
}

bool RingBufferWrapper::HasPendingUpload() const
{
    // Ring buffers don't use staging, so always false
    return false;
}

void RingBufferWrapper::MarkDirty()
{
    dirty = true;
}

bool RingBufferWrapper::CommitInternal()
{
    if(!dirty)
        return false;

    // Ring buffer data is immediately in device buffer (no staging)
    // Just clear the dirty flag
    ClearDirty();
    return true;
}

DeviceBuffer* RingBufferWrapper::GetBuffer()
{
    return writer.GetBuffer();
}

const DeviceBuffer* RingBufferWrapper::GetBuffer() const
{
    return writer.GetBuffer();
}
}//namespace hgl::graph
