#pragma once

#include<hgl/vk/VKBufferOwner.h>

namespace hgl::graph{
class VertexAttribBuffer:public VkBufferOwner
{
    VkFormat format;                    ///<数据格式
    uint32_t stride;                    ///<单个数据字节数
    uint32_t count;                     ///<数据数量

private:

    friend class VulkanDevice;

    VertexAttribBuffer(VkDevice d,const DeviceBufferData &vb,VkFormat fmt,uint32_t _stride,uint32_t _count):VkBufferOwner(d,vb)
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

    void *  Map     (VkDeviceSize start,VkDeviceSize size)          {return GetGPUBuffer()->Map(start*stride,size*stride);}
    void    Unmap   ()                                              {GetGPUBuffer()->Unmap();}
    void    Flush   (VkDeviceSize start,VkDeviceSize size)          {GetGPUBuffer()->MarkDirty(start*stride,size*stride);}
    void    Flush   (VkDeviceSize size)                             {GetGPUBuffer()->MarkDirty(0,size*stride);}

    bool    Write   (const void *ptr,uint32_t start,uint32_t size)  {return GetGPUBuffer()->Write(ptr,start*stride,size*stride);}
    bool    Write   (const void *ptr,uint32_t size)                 {return GetGPUBuffer()->Write(ptr,0,size*stride);}

    /**
     * Returns the underlying VkBuffer handle for draw/bind calls.
     * Prefer this over GetBuffer() for VkCmdBindVertexBuffers — does not require VKDescriptorBuffer inheritance.
     */
    VkBuffer GetVkBuffer() const { return GetGPUBuffer()->GetVkDeviceBuffer(); }
};//class VertexAttribBuffer:public VkBufferOwner

using VAB=VertexAttribBuffer;

}//namespace hgl::graph
