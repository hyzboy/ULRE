#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKMemory.h>
#include<hgl/vk/BufferPolicy.h>
#include<hgl/graph/mtl/ShaderBufferSource.h>

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