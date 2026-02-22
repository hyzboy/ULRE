#include<hgl/vk/VKBufferTransferAgent.h>
#include<hgl/vk/VKStagedBuffer.h>
#include<hgl/vk/VKBuffer.h>

namespace hgl::graph
{
// Legacy constructor for backward compatibility
StagedBufferTransferAgent::StagedBufferTransferAgent(StagedBuffer *sb)
{
    staged_buffer = sb;
    device_buffer = nullptr;
}

// New constructor with DeviceBuffer reference
StagedBufferTransferAgent::StagedBufferTransferAgent(StagedBuffer *sb, DeviceBuffer *db)
{
    staged_buffer = sb;
    device_buffer = db;
}

StagedBufferTransferAgent::~StagedBufferTransferAgent()
{
    delete staged_buffer;
    staged_buffer = nullptr;
    device_buffer = nullptr;
}

bool StagedBufferTransferAgent::HasPendingUpload() const
{
    return staged_buffer ? staged_buffer->IsDirty() : false;
}

void *StagedBufferTransferAgent::Map(VkDeviceSize start, VkDeviceSize size)
{
    if(!staged_buffer)
        return nullptr;

    staged_map_offset = start;
    staged_map_size = (size == 0) ? VK_WHOLE_SIZE : size;
    staged_map_active = true;
    return staged_buffer->Map(start, size);
}

void StagedBufferTransferAgent::Unmap()
{
    if(!staged_buffer)
        return;

    staged_buffer->Unmap();
    if(staged_map_active)
    {
        staged_buffer->MarkDirty(staged_map_offset, staged_map_size);
        is_dirty = true;
        staged_map_active = false;
    }
}

void StagedBufferTransferAgent::Flush(VkDeviceSize start, VkDeviceSize size)
{
    if(!staged_buffer)
        return;

    staged_buffer->MarkDirty(start, (size == 0) ? VK_WHOLE_SIZE : size);
    is_dirty = true;
}

bool StagedBufferTransferAgent::Write(const void *ptr, uint32_t start, uint32_t size)
{
    if(!staged_buffer)
        return false;

    bool result = staged_buffer->Write(ptr, static_cast<VkDeviceSize>(start), static_cast<VkDeviceSize>(size));
    if(result)
        is_dirty = true;
    return result;
}

// BufferWriteAgent implementation
void* StagedBufferTransferAgent::MapRange(VkDeviceSize offset, VkDeviceSize count)
{
    return Map(offset, count);
}

bool StagedBufferTransferAgent::WriteRange(const void *data, VkDeviceSize offset, VkDeviceSize count)
{
    return Write(data, static_cast<uint32_t>(offset), static_cast<uint32_t>(count));
}

void StagedBufferTransferAgent::MarkDirty()
{
    is_dirty = true;
    if(staged_buffer)
        staged_buffer->MarkDirty();
}

bool StagedBufferTransferAgent::IsDirty() const
{
    return is_dirty;
}

bool StagedBufferTransferAgent::CommitInternal()
{
    if(!is_dirty || !staged_buffer)
        return false;

    // Typically called by BufferCommitQueue during RenderBufferCommit phase
    // For staged buffers, we've already marked it dirty during Write/Unmap
    // The actual copy to DeviceBuffer happens in RenderBufferUploadSystem
    is_dirty = false;
    return true;
}

DeviceBuffer* StagedBufferTransferAgent::GetBuffer()
{
    return device_buffer;
}

const DeviceBuffer* StagedBufferTransferAgent::GetBuffer() const
{
    return device_buffer;
}
}//namespace hgl::graph
