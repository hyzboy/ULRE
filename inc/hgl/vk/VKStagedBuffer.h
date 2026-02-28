#ifndef HGL_GRAPH_VULKAN_STAGED_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_STAGED_BUFFER_INCLUDE

#include<hgl/vk/VK.h>
#include<hgl/vk/VKMemory.h>
#include<hgl/vk/IGPUBuffer.h>
#include<vector>

namespace hgl::graph{

class DeviceMemory;

/**
 * Staged buffer with CPU-visible staging buffer and GPU-local device buffer
 * Implements IGPUBuffer — ECS System calls CopyToDevice each frame for dirty buffers
 */
class StagedBuffer : public IGPUBuffer
{
    VkDevice            device;

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
    std::vector<DirtyRange> dirty_ranges;

    // Tracks the last Map() range so Unmap() can dirty only what was actually mapped
    VkDeviceSize        mapped_offset = 0;
    VkDeviceSize        mapped_size   = 0;

private:

    friend class VulkanDevice;

    StagedBuffer(const std::string &name, VkDevice dev, VkBuffer staging_buf, DeviceMemory *staging_mem,
                 VkBuffer device_buf, DeviceMemory *device_mem, VkDeviceSize size, VkBufferUsageFlags usage_flags);

public:

    virtual ~StagedBuffer();

    // ---- IGPUBuffer interface ----
    bool   Write      (const void *data, VkDeviceSize offset, VkDeviceSize size) override;
    bool   Write      (const void *data, VkDeviceSize size) { return Write(data, 0, size); }
    bool   Write      (const void *data)                    { return Write(data, 0, buffer_size); }

    void * Map        (VkDeviceSize offset, VkDeviceSize size) override;
    void * Map        ();
    void   Unmap      () override;

    void   MarkDirty  (VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) override;
    void   MarkDirtyRanges(const DirtyRange *ranges, size_t count) override;
    bool   IsDirty    () const override { return is_dirty; }
    void   ClearDirty () override;

    /** Called by RenderBufferUploadSystem each frame for dirty buffers */
    void   CopyToDevice(VkCommandBuffer cmd) override;

    VkDeviceSize GetSize()            const override { return buffer_size; }
    VkBuffer     GetVkDeviceBuffer()  const override { return device_buffer; }
    VkBuffer     GetStagingBuffer()   const          { return staging_buffer; }
    DeviceMemory *GetDeviceMemory()   const          { return device_memory; }

    VkDescriptorBufferInfo GetDescriptorBufferInfo() const override
    {
        VkDescriptorBufferInfo info{};
        info.buffer = device_buffer;
        info.offset = 0;
        info.range  = buffer_size;
        return info;
    }
};//class StagedBuffer

}//namespace hgl::graph
#endif//HGL_GRAPH_VULKAN_STAGED_BUFFER_INCLUDE
