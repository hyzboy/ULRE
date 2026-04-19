#pragma once

#include<hgl/vk/VKBufferOwner.h>          // defines VkBufferOwner + DeviceBufferData
#include<string>

namespace hgl::graph
{
/**
 * Layer1: Pure GPU buffer container for UBO / SSBO / plain device buffers.
 *
 * Inherits VkBufferOwner which holds: VkDevice, DeviceBufferData (VkBuffer+DeviceMemory),
 * IGPUBuffer* staged_source (write routing), and BufferUpdateClass.
 *
 * VertexAttribBuffer, IndexBuffer, IndirectCommandBuffer now inherit VkBufferOwner
 * directly — they are no longer subclasses of DeviceBuffer.
 *
 * Migration path (Phase 3):
 *   - Prefer GetGPUBuffer()->Write/Map/Unmap over VKDescriptorBuffer::Write/Map/Flush.
 *   - VKDescriptorBuffer::Write/Map/Flush are transitional forwarders; will be removed.
 */
class VKDescriptorBuffer : public VkBufferOwner
{
private:

    friend class VulkanDevice;

    VKDescriptorBuffer(VkDevice d, const DeviceBufferData &b) : VkBufferOwner(d, b) {}

public:

    // Destructor declared out-of-line so old stale OBJ files that reference
    // the virtual ~VKDescriptorBuffer() symbol can still link against VKBuffer.obj.
    virtual ~VKDescriptorBuffer()=default;

    //// Phase 2 decision (2026-04-19): keep FlushRanges on VKDescriptorBuffer.
    //// Rationale: IGPUBuffer currently exposes MarkDirty/MarkDirtyRanges, not FlushRanges.
    //// Future migration to IGPUBuffer requires interface extension first, then caller migration.
    virtual void    FlushRanges(const IGPUBuffer::DirtyRange *ranges,size_t count);
};//class VKDescriptorBuffer

}//namespace hgl::graph
