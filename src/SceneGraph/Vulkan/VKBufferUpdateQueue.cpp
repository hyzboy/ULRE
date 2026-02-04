#include<hgl/graph/VKBufferUpdateQueue.h>
#include<hgl/graph/VKStagedBuffer.h>

VK_NAMESPACE_BEGIN

BufferUpdateQueue::BufferUpdateQueue(VkDevice dev)
{
    device = dev;
}

BufferUpdateQueue::~BufferUpdateQueue()
{
    Clear();
}

void BufferUpdateQueue::AddUpdate(StagedBuffer *buffer, VkDeviceSize offset, VkDeviceSize size)
{
    if (!buffer)
        return;

    // Check if buffer is already in the queue to avoid duplicates
    for (int i = 0; i < pending_updates.GetCount(); i++)
    {
        if (pending_updates[i].buffer == buffer)
        {
            // Update existing record with combined range
            BufferUpdateRecord &record = pending_updates[i];
            
            VkDeviceSize new_offset = hgl_min(record.offset, offset);
            VkDeviceSize record_end = (record.size == VK_WHOLE_SIZE) ? buffer->GetSize() : (record.offset + record.size);
            VkDeviceSize new_end = (size == VK_WHOLE_SIZE) ? buffer->GetSize() : (offset + size);
            VkDeviceSize new_size = hgl_max(record_end, new_end) - new_offset;
            
            record.offset = new_offset;
            record.size = new_size;
            return;
        }
    }

    // Add new record
    pending_updates.Add(BufferUpdateRecord(buffer, offset, size));
}

void BufferUpdateQueue::FlushAll(VkCommandBuffer cmd)
{
    if (!cmd || pending_updates.GetCount() == 0)
        return;

    // Execute all pending copies
    for (int i = 0; i < pending_updates.GetCount(); i++)
    {
        BufferUpdateRecord &record = pending_updates[i];
        if (record.buffer && record.buffer->IsDirty())
        {
            record.buffer->CopyToDevice(cmd);
        }
    }

    Clear();
}

void BufferUpdateQueue::Clear()
{
    pending_updates.Clear();
}

VK_NAMESPACE_END
