#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKMemory.h>
#include<hgl/vk/BufferPolicy.h>
#include<hgl/graph/mtl/ShaderBufferSource.h>
#include<hgl/vk/IGPUBuffer.h>

#include<string>

namespace hgl::graph{

struct DeviceBufferData
{
    VkBuffer                buffer=nullptr;
    DeviceMemory *          memory=nullptr;
    VkDescriptorBufferInfo  info;
};//struct DeviceBufferData

/**
 * Layer1: Pure GPU buffer container.
 * Wraps VkBuffer + DeviceMemory. Contains no ECS, no policy, no queue logic.
 *
 * If staged_source is set (factory assigned), write/map/flush operations are routed
 * through that IGPUBuffer implementation:
 *   StagedBuffer  — staging(CPU) + device(GPU), CopyToDevice uploads each frame.
 *   ReBarBuffer   — single CPU-visible/device-local allocation, CopyToDevice is no-op.
 *
 * The staged_source is owned by this DeviceBuffer and deleted in the destructor.
 *
 * Migration path (Phase 3):
 *   - Prefer GetGPUBuffer()->Write/Map/Unmap over DeviceBuffer::Write/Map/Flush
 *     for new code.  The DeviceBuffer::Write/Map/Flush overloads are transitional
 *     forwarders and will be removed once all call sites migrate to IGPUBuffer*.
 *   - GetGPUBuffer() returns nullptr only for pure device-local buffers (no upload path).
 *     For StagedUpload or CPUVisible buffers it always returns a valid IGPUBuffer*.
 */
class DeviceBuffer
{
protected:

    VkDevice device;
    DeviceBufferData buf;

    // Owns the IGPUBuffer implementation for write routing + lifecycle.
    // Set by factory: StagedBuffer (staged-upload) or ReBarBuffer (CPUVisible/ReBAR).
    // nullptr only for pure device-local buffers with no CPU write path.
    IGPUBuffer *staged_source = nullptr;

    // Update class for ECS routing hint
    BufferUpdateClass update_class = BufferUpdateClass::Default;

private:

    friend class VulkanDevice;
    friend class VertexAttribBuffer;
    friend class IndexBuffer;
    template<typename T> friend class IndirectCommandBuffer;

    DeviceBuffer(VkDevice d, const DeviceBufferData &b)
    {
        device = d;
        buf    = b;
    }

public:

    virtual ~DeviceBuffer();

            VkBuffer                    GetBuffer   ()const{return buf.buffer;}
            DeviceMemory *              GetMemory   ()const{return buf.memory;}
            VkDeviceMemory              GetVkMemory ()const{return buf.memory->operator VkDeviceMemory();}
    const   VkDescriptorBufferInfo *    GetBufferInfo()const{return &buf.info;}
            VkDeviceSize                GetSize     ()const{return buf.info.range;}

    // Assign ownership of IGPUBuffer impl (StagedBuffer or ReBarBuffer) for write
    // routing and lifecycle management.
    void        SetStagedSource(IGPUBuffer *s) { staged_source = s; }
    IGPUBuffer *GetStagedSource()const         { return staged_source; }

    void              SetUpdateClass(BufferUpdateClass cls){update_class=cls;}
    BufferUpdateClass GetUpdateClass()const{return update_class;}

    /**
     * Returns the IGPUBuffer interface for CPU writes and dirty tracking.
     * Non-null for StagedUpload (StagedBuffer) and CPUVisible (ReBarBuffer) buffers.
     * nullptr only for pure device-local buffers (no upload path configured).
     * Phase 3 migration: prefer this over Write/Map/Flush for new code.
     */
    IGPUBuffer *      GetGPUBuffer()       { return staged_source; }
    const IGPUBuffer *GetGPUBuffer() const { return staged_source; }

    // Transitional forwarders — route through staged_source when present.
    // New code should use GetGPUBuffer()->Write/Map/Unmap instead.
            void *  Map     ();
    virtual void *  Map     (VkDeviceSize start,VkDeviceSize size);
            void    Unmap   ();
    virtual void    Flush   (VkDeviceSize start,VkDeviceSize size);
    virtual void    Flush   (VkDeviceSize size);

    virtual bool    Write   (const void *ptr,uint32_t start,uint32_t size);
    virtual bool    Write   (const void *ptr,uint32_t size);
            bool    Write   (const void *ptr);

};//class DeviceBuffer

}//namespace hgl::graph