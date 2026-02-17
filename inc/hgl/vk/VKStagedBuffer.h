#ifndef HGL_GRAPH_VULKAN_STAGED_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_STAGED_BUFFER_INCLUDE

#include<hgl/vk/VK.h>
#include<hgl/vk/VKMemory.h>

namespace hgl::graph{

class BufferUpdateQueue;
class DeviceMemory;

/**
 * Staged buffer with CPU-visible staging buffer and GPU-local device buffer
 */
class StagedBuffer
{
    VkDevice            device;
    BufferUpdateQueue * update_queue;

    // Staging buffer (CPU accessible)
    VkBuffer            staging_buffer;
    DeviceMemory *      staging_memory;

    // Device buffer (GPU optimal)
    VkBuffer            device_buffer;
    DeviceMemory *      device_memory;

    VkDeviceSize        buffer_size;
    VkBufferUsageFlags  usage;

    bool                is_dirty;
    VkDeviceSize        dirty_offset;
    VkDeviceSize        dirty_size;

private:

    friend class VulkanDevice;

    StagedBuffer(VkDevice dev, BufferUpdateQueue *queue, VkBuffer staging_buf, DeviceMemory *staging_mem,
                 VkBuffer device_buf, DeviceMemory *device_mem, VkDeviceSize size, VkBufferUsageFlags usage_flags);

public:

    virtual ~StagedBuffer();

    /**
     * Write data to staging buffer and mark as dirty
     */
    bool Write(const void *data, VkDeviceSize offset, VkDeviceSize size);
    bool Write(const void *data, VkDeviceSize size) { return Write(data, 0, size); }
    bool Write(const void *data) { return Write(data, 0, buffer_size); }

    /**
     * Map staging buffer for CPU access
     */
    void * Map();
    void * Map(VkDeviceSize offset, VkDeviceSize size);
    void   Unmap();

    /**
     * Mark buffer as dirty (to be copied to device)
     */
    void MarkDirty(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

    /**
     * Copy staging buffer to device buffer (called by BufferUpdateQueue)
     */
    void CopyToDevice(VkCommandBuffer cmd);

    /**
     * Get the device buffer for rendering
     */
    VkBuffer GetDeviceBuffer() const { return device_buffer; }
    VkBuffer GetStagingBuffer() const { return staging_buffer; }
    DeviceMemory *GetDeviceMemory() const { return device_memory; }

    VkDeviceSize GetSize() const { return buffer_size; }
    bool IsDirty() const { return is_dirty; }

    /**
     * Clear dirty flag (called after copy)
     */
    void ClearDirty();
};//class StagedBuffer

}//namespace hgl::graph
#endif//HGL_GRAPH_VULKAN_STAGED_BUFFER_INCLUDE
