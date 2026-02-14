#pragma once

#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/VKBufferMap.h>

VK_NAMESPACE_BEGIN
class VertexAttribBuffer:public DeviceBuffer
{
    VkFormat format;                    ///<数据格式
    uint32_t stride;                    ///<单个数据字节数
    uint32_t count;                     ///<数据数量

private:

    friend class VulkanDevice;

    VertexAttribBuffer(VulkanDevice *owner,VkDevice d,const DeviceBufferData &vb,VkFormat fmt,uint32_t _stride,uint32_t _count):DeviceBuffer(owner,d,vb)
    {
        format=fmt;
        stride=_stride;
        count=_count;
    }

    VertexAttribBuffer(VulkanDevice *owner,VkDevice d,const DeviceBufferData &vb,VkFormat fmt,uint32_t _stride,uint32_t _count,StagedBuffer *sb):DeviceBuffer(owner,d,vb,sb)
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

    void *  Map     (VkDeviceSize start,VkDeviceSize size)          override {return DeviceBuffer::Map(start*stride,size*stride);}
    void    Flush   (VkDeviceSize start,VkDeviceSize size)          override {return DeviceBuffer::Flush(start*stride,size*stride);}
    void    Flush   (VkDeviceSize size)                             override {return DeviceBuffer::Flush(size*stride);}

    bool    Write   (const void *ptr,uint32_t start,uint32_t size)  override {return DeviceBuffer::Write(ptr,start*stride,size*stride);}
    bool    Write   (const void *ptr,uint32_t size)                 override {return DeviceBuffer::Write(ptr,0,size*stride);}
};//class VertexAttribBuffer:public DeviceBuffer

using VAB=VertexAttribBuffer;

class VABMap:public VKBufferMap<VAB>
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

VK_NAMESPACE_END
