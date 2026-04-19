#ifndef HGL_GRAPH_VULKAN_STAGED_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_STAGED_BUFFER_INCLUDE

#include<hgl/vk/VK.h>
#include<hgl/vk/IGPUBuffer.h>
#include<vk_mem_alloc.h>
#include<vector>

namespace hgl::graph{

/**
 * Staged buffer with CPU-visible staging buffer and GPU-local device buffer
 * Implements IGPUBuffer — ECS System calls CopyToDevice each frame for dirty buffers
 */
class StagedBuffer : public IGPUBuffer
{
    VkDevice            device;
    VmaAllocator        allocator = VK_NULL_HANDLE;

    // Staging buffer (CPU accessible)
    VkBuffer            staging_buffer = VK_NULL_HANDLE;
    VmaAllocation       staging_allocation = VK_NULL_HANDLE;
    VkDeviceMemory      staging_vk_memory = VK_NULL_HANDLE;

    // Device buffer (GPU optimal)
    VkBuffer            device_buffer = VK_NULL_HANDLE;
    VmaAllocation       device_allocation = VK_NULL_HANDLE;
    VkDeviceMemory      device_vk_memory = VK_NULL_HANDLE;

    VkDeviceSize        buffer_size = 0;
    VkBufferUsageFlags  usage = 0;

    // Tracks the last Map() range so Unmap() can dirty only what was actually mapped
    VkDeviceSize        mapped_offset = 0;
    VkDeviceSize        mapped_size   = 0;
    void               *mapped_ptr    = nullptr;

private:

    friend class VulkanDevice;

    StagedBuffer(const std::string &name,
                 VkDevice dev,
                 VmaAllocator alloc,
                 VkBuffer staging_buf,
                 VmaAllocation staging_alloc,
                 VkDeviceMemory staging_mem,
                 VkBuffer device_buf,
                 VmaAllocation device_alloc,
                 VkDeviceMemory device_mem,
                 VkDeviceSize size,
                 VkBufferUsageFlags usage_flags);

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
    bool   IsDirty    () const override { return HasTrackedDirty(); }
    void   ClearDirty () override;

    /** Called by RenderBufferUploadSystem each frame for dirty buffers */
    void   CopyToDevice(VkCommandBuffer cmd) override;

    VkDeviceSize GetSize()            const override { return buffer_size; }
    VkBuffer     GetVkDeviceBuffer()  const override { return device_buffer; }
    VkBuffer     GetStagingBuffer()   const          { return staging_buffer; }
    VkDeviceMemory GetVkDeviceMemory() const         { return device_vk_memory; }

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
