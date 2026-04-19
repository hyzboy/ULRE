#pragma once

#include<hgl/vk/VKMemory.h>
#include<hgl/vk/BufferPolicy.h>
#include<hgl/vk/IGPUBuffer.h>
#include<hgl/vk/VK.h>
#include<vk_mem_alloc.h>

namespace hgl::graph{

/**
 * Aggregate holding Vulkan handles owned by any GPU buffer.
 */
struct DeviceBufferData
{
    VkBuffer                buffer      = nullptr;
    VmaAllocation           allocation  = VK_NULL_HANDLE;
    VkDeviceMemory          vk_memory   = VK_NULL_HANDLE; // alias for tracking/logging only
    VkDescriptorBufferInfo  info;
};//struct DeviceBufferData

class VkBufferOwner
{
protected:

    VkDevice         device        = VK_NULL_HANDLE;
    DeviceBufferData buf;

    IGPUBuffer      *staged_source = nullptr;

    BufferUpdateClass update_class = BufferUpdateClass::Default;

    friend class VulkanDevice;

    VkBufferOwner() = default;

    VkBufferOwner(VkDevice d, const DeviceBufferData &b) : device(d), buf(b) {}

public:

    virtual ~VkBufferOwner();

            VkBuffer                    GetBuffer    () const { return buf.buffer; }
            VmaAllocation               GetAllocation() const { return buf.allocation; }
            VkDeviceMemory              GetVkMemory  () const { return buf.vk_memory; }
    const   VkDescriptorBufferInfo     *GetBufferInfo() const { return &buf.info; }
            VkDeviceSize                GetSize      () const { return buf.info.range; }

    void        SetStagedSource(IGPUBuffer *s)  { staged_source = s; }
    IGPUBuffer *GetStagedSource()         const { return staged_source; }

    IGPUBuffer       *GetGPUBuffer()       { return staged_source; }
    const IGPUBuffer *GetGPUBuffer() const { return staged_source; }

    void              SetUpdateClass(BufferUpdateClass c) { update_class = c; }
    BufferUpdateClass GetUpdateClass()               const { return update_class; }

};//class VkBufferOwner

}//namespace hgl::graph
