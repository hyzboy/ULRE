#pragma once

#include<hgl/vk/VKBufferOwner.h>          // defines VkBufferOwner + DeviceBufferData
#include<hgl/mtl/ShaderBufferSource.h>

#include<string>

namespace hgl::graph{

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
 *   - Prefer GetGPUBuffer()->Write/Map/Unmap over DeviceBuffer::Write/Map/Flush.
 *   - DeviceBuffer::Write/Map/Flush are transitional forwarders; will be removed.
 */
class DeviceBuffer : public VkBufferOwner
{
private:

    friend class VulkanDevice;

    DeviceBuffer(VkDevice d, const DeviceBufferData &b) : VkBufferOwner(d, b) {}

public:

    // Destructor declared out-of-line so old stale OBJ files that reference
    // the virtual ~DeviceBuffer() symbol can still link against VKBuffer.obj.
    virtual ~DeviceBuffer();

    // Transitional forwarders — route through staged_source when present.
    // TODO(Phase3c): Remove once VKMemoryAllocator is migrated or isolated.
    // NOTE: [[deprecated]] intentionally NOT on virtual overloads — MSVC changes
    // symbol mangling for deprecated virtuals which breaks linking.
    [[deprecated("Use GetGPUBuffer()->Map(0,GetSize()) instead.")]]
            void *  Map     ();
    virtual void *  Map     (VkDeviceSize start,VkDeviceSize size);
    [[deprecated("Use GetGPUBuffer()->Unmap() instead.")]]
            void    Unmap   ();
    virtual void    Flush   (VkDeviceSize start,VkDeviceSize size);
    virtual void    Flush   (VkDeviceSize size);
        virtual void    FlushRanges(const IGPUBuffer::DirtyRange *ranges,size_t count);

    virtual bool    Write   (const void *ptr,uint32_t start,uint32_t size);
    virtual bool    Write   (const void *ptr,uint32_t size);
    [[deprecated("Use GetGPUBuffer()->Write(ptr,0,GetSize()) instead.")]]
            bool    Write   (const void *ptr);

};//class DeviceBuffer

}//namespace hgl::graph