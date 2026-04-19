#pragma once

#include<hgl/vk/IGPUBuffer.h>
#include<vk_mem_alloc.h>
#include<cstring>

namespace hgl::graph{

class ReBarBuffer : public IGPUBuffer
{
    VmaAllocator  allocator  = VK_NULL_HANDLE;
    VkBuffer      buffer     = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDeviceSize  buf_size   = 0;
    void         *mapped_ptr = nullptr;

private:
    friend class VulkanDevice;

    ReBarBuffer(const std::string &name,
                VmaAllocator       alloc,
                VkBuffer           buf,
                VmaAllocation      alloc_handle,
                VkDeviceSize       size)
        : IGPUBuffer(name)
        , allocator(alloc)
        , buffer(buf)
        , allocation(alloc_handle)
        , buf_size(size)
    {}

public:
    virtual ~ReBarBuffer() override
    {
        if(mapped_ptr)
            vmaUnmapMemory(allocator, allocation);

        if(buffer && allocation)
            vmaDestroyBuffer(allocator, buffer, allocation);
    }

    bool Write(const void *data, VkDeviceSize offset, VkDeviceSize size) override
    {
        if(!data || !allocation || offset + size > buf_size)
            return false;

        void *ptr = nullptr;
        if(vmaMapMemory(allocator, allocation, &ptr) != VK_SUCCESS || !ptr)
            return false;

        std::memcpy(static_cast<char *>(ptr) + offset, data, static_cast<size_t>(size));
        vmaUnmapMemory(allocator, allocation);
        return true;
    }

    void *Map(VkDeviceSize offset, VkDeviceSize size) override
    {
        if(!allocation || offset + size > buf_size)
            return nullptr;

        if(mapped_ptr)
            return static_cast<char *>(mapped_ptr) + offset;

        if(vmaMapMemory(allocator, allocation, &mapped_ptr) != VK_SUCCESS || !mapped_ptr)
            return nullptr;

        return static_cast<char *>(mapped_ptr) + offset;
    }

    void Unmap() override
    {
        if(mapped_ptr)
        {
            vmaUnmapMemory(allocator, allocation);
            mapped_ptr = nullptr;
        }
    }

    void MarkDirty (VkDeviceSize = 0, VkDeviceSize = VK_WHOLE_SIZE) override {}
    void MarkDirtyRanges(const DirtyRange *, size_t) override {}
    bool IsDirty   () const override { return false; }
    void ClearDirty() override {}

    void CopyToDevice(VkCommandBuffer /*cmd*/) override {}

    VkDeviceSize GetSize()           const override { return buf_size; }
    VkBuffer     GetVkDeviceBuffer() const override { return buffer; }

    VkDescriptorBufferInfo GetDescriptorBufferInfo() const override
    {
        VkDescriptorBufferInfo info{};
        info.buffer = buffer;
        info.offset = 0;
        info.range  = buf_size;
        return info;
    }
};//class ReBarBuffer

}//namespace hgl::graph
