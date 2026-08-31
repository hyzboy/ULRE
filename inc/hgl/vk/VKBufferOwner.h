#pragma once

#include<hgl/vk/VKMemory.h>
#include<hgl/vk/BufferPolicy.h>
#include<hgl/vk/IGPUBuffer.h>
#include<hgl/vk/VK.h>

namespace hgl::graph{

/**
 * Aggregate holding the three Vulkan handles owned by any GPU buffer.
 * Formerly defined inside VKBuffer.h; moved here so VkBufferOwner can use it
 * without creating a circular dependency.
 */
struct DeviceBufferData
{
    VkBuffer                buffer  = nullptr;
    DeviceMemory           *memory  = nullptr;
    VkDescriptorBufferInfo  info;
};//struct DeviceBufferData

/**
 * VkBufferOwner — thin base class for all GPU buffer types.
 *
 * Owns: VkBuffer + DeviceMemory + IGPUBuffer (upload path).
 * Shared by DeviceBuffer (UBO/SSBO), VertexAttribBuffer, IndexBuffer, IndirectCommandBuffer<T>.
 *
 * Destructor: if staged_source is set, delegates cleanup to it (it owns the allocations).
 *             Otherwise destroys buf.buffer via vkDestroyBuffer and deletes buf.memory.
 */
class VkBufferOwner
{
protected:

    VkDevice         device        = VK_NULL_HANDLE;
    DeviceBufferData buf;

    // Owns the IGPUBuffer implementation for write routing + lifecycle.
    // Set by factory via SetStagedSource(). nullptr only for pure device-local buffers.
    IGPUBuffer      *staged_source = nullptr;

    // ECS routing hint — set by factory via SetUpdateClass.
    BufferUpdateClass update_class = BufferUpdateClass::Default;

    VkBufferOwner() = default;

    VkBufferOwner(VkDevice d, const DeviceBufferData &b) : device(d), buf(b) {}

public:

    virtual ~VkBufferOwner();

            VkBuffer                    GetBuffer    () const { return buf.buffer; }
            DeviceMemory               *GetMemory    () const { return buf.memory; }
            VkDeviceMemory              GetVkMemory  () const { return buf.memory->operator VkDeviceMemory(); }
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
