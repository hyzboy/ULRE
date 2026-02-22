#include<hgl/vk/VKBufferTransferAgent.h>
#include<hgl/vk/VKStagedBuffer.h>

namespace hgl::graph
{
StagedBufferTransferAgent::StagedBufferTransferAgent(StagedBuffer *sb)
{
    staged_buffer = sb;
}

StagedBufferTransferAgent::~StagedBufferTransferAgent()
{
    delete staged_buffer;
    staged_buffer = nullptr;
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
        staged_map_active = false;
    }
}

void StagedBufferTransferAgent::Flush(VkDeviceSize start, VkDeviceSize size)
{
    if(!staged_buffer)
        return;

    staged_buffer->MarkDirty(start, (size == 0) ? VK_WHOLE_SIZE : size);
}

bool StagedBufferTransferAgent::Write(const void *ptr, uint32_t start, uint32_t size)
{
    if(!staged_buffer)
        return false;

    return staged_buffer->Write(ptr, static_cast<VkDeviceSize>(start), static_cast<VkDeviceSize>(size));
}
}//namespace hgl::graph
