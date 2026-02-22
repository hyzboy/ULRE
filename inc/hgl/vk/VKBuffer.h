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

class StagedBuffer;

/**
 * Layer1: Pure GPU buffer container.
 * Wraps VkBuffer + DeviceMemory. Contains no ECS, no policy, no queue logic.
 *
 * If staged_source is set (factory assigned), write/map/flush operations are routed
 * through the StagedBuffer (CPU-visible staging → GPU copy on demand).
 * The StagedBuffer is owned by this DeviceBuffer and deleted in the destructor.
 *
 * Migration path (Phase 3):
 *   - Prefer GetGPUBuffer()->Write/Map/Unmap over DeviceBuffer::Write/Map/Flush
 *     for new code.  The DeviceBuffer::Write/Map/Flush overloads are transitional
 *     forwarders and will be removed once all call sites migrate to IGPUBuffer*.
 *   - GetGPUBuffer() returns nullptr for pure device-local buffers (no CPU write path).
 */
class DeviceBuffer
{
protected:

    VkDevice device;
    DeviceBufferData buf;

    // Owns the StagedBuffer for staged-upload path (write routing + lifecycle)
    StagedBuffer *staged_source = nullptr;

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

    // Assign ownership of StagedBuffer for write routing and lifecycle management
    void              SetStagedSource(StagedBuffer *s) { staged_source = s; }
    StagedBuffer *    GetStagedSource()const            { return staged_source; }

    void              SetUpdateClass(BufferUpdateClass cls){update_class=cls;}
    BufferUpdateClass GetUpdateClass()const{return update_class;}

    /**
     * Returns the IGPUBuffer interface for CPU writes and dirty tracking.
     * nullptr for pure device-local buffers (no upload path configured).
     * Phase 3 migration: prefer this over Write/Map/Flush for new code.
     */
    IGPUBuffer *      GetGPUBuffer();
    const IGPUBuffer *GetGPUBuffer() const;

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