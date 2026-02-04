#ifndef HGL_GRAPH_VULKAN_BUFFER_UPDATE_QUEUE_INCLUDE
#define HGL_GRAPH_VULKAN_BUFFER_UPDATE_QUEUE_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/List.h>

VK_NAMESPACE_BEGIN

class StagedBuffer;

/**
 * Update record for a staged buffer
 */
struct BufferUpdateRecord
{
    StagedBuffer *  buffer;
    VkDeviceSize    offset;
    VkDeviceSize    size;

    BufferUpdateRecord(StagedBuffer *buf, VkDeviceSize off = 0, VkDeviceSize sz = VK_WHOLE_SIZE)
        : buffer(buf), offset(off), size(sz) {}
};

/**
 * Manages a queue of buffer updates to be flushed in batch
 */
class BufferUpdateQueue
{
    VkDevice device;
    List<BufferUpdateRecord> pending_updates;

public:

    BufferUpdateQueue(VkDevice dev);
    ~BufferUpdateQueue();

    /**
     * Add a buffer update to the queue
     */
    void AddUpdate(StagedBuffer *buffer, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

    /**
     * Check if there are pending updates
     */
    bool HasPendingUpdates() const { return pending_updates.GetCount() > 0; }

    /**
     * Flush all pending updates to GPU
     * @param cmd Command buffer to record copy commands
     */
    void FlushAll(VkCommandBuffer cmd);

    /**
     * Clear all pending updates without flushing
     */
    void Clear();
};//class BufferUpdateQueue

VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_BUFFER_UPDATE_QUEUE_INCLUDE
