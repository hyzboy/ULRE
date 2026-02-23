#pragma once

#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/VKBufferMap.h>

namespace hgl::graph{
class VertexAttribBuffer:public DeviceBuffer
{
    VkFormat format;                    ///<数据格式
    uint32_t stride;                    ///<单个数据字节数
    uint32_t count;                     ///<数据数量

private:

    friend class VulkanDevice;

    VertexAttribBuffer(VkDevice d,const DeviceBufferData &vb,VkFormat fmt,uint32_t _stride,uint32_t _count):DeviceBuffer(d,vb)
    {
        format=fmt;
        stride=_stride;
        count=_count;
    }

public:

    ~VertexAttribBuffer()=default;

    const VkFormat GetFormat()const { return format; }
    const uint32_t GetStride()const { return stride; }
    const uint32_t GetCount ()const { return count; }

    const VkDeviceSize GetTotalBytes()const { return stride*count; }

public:

    void *  Map     (VkDeviceSize start,VkDeviceSize size)          override {return GetGPUBuffer()->Map(start*stride,size*stride);}
    void    Unmap   ()                                                      {GetGPUBuffer()->Unmap();}
    void    Flush   (VkDeviceSize start,VkDeviceSize size)          override {GetGPUBuffer()->MarkDirty(start*stride,size*stride);}
    void    Flush   (VkDeviceSize size)                             override {GetGPUBuffer()->MarkDirty(0,size*stride);}

    bool    Write   (const void *ptr,uint32_t start,uint32_t size)  override {return GetGPUBuffer()->Write(ptr,start*stride,size*stride);}
    bool    Write   (const void *ptr,uint32_t size)                 override {return GetGPUBuffer()->Write(ptr,0,size*stride);}

    /**
     * Returns the underlying VkBuffer handle for draw/bind calls.
     * Prefer this over GetBuffer() for VkCmdBindVertexBuffers — does not require DeviceBuffer inheritance.
     */
    VkBuffer GetVkBuffer() const { return GetGPUBuffer()->GetVkDeviceBuffer(); }
};//class VertexAttribBuffer:public DeviceBuffer

using VAB=VertexAttribBuffer;

// DEPRECATED: Use BufferAccessor<VAB> instead.
class [[deprecated("VABMap is superseded by BufferAccessor<VAB>. See VKBufferAccessor.h.")]] VABMap:public VKBufferMap<VAB>
{
public:

    using VKBufferMap<VAB>::VKBufferMap;
    virtual ~VABMap()=default;

    const VkFormat GetFormat()const { return buffer->GetFormat(); }

    void BindVAB(VAB *vab,const VkDeviceSize off,const uint32_t count)
    {
        VKBufferMap<VAB>::Bind(vab,off,vab->GetStride(),count);
    }
};//class VABMap

}//namespace hgl::graph
