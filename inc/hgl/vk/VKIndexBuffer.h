#pragma once

#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/VKBufferMap.h>

namespace hgl::graph{
class IndexBuffer:public DeviceBuffer
{
    IndexType   index_type;
    uint        stride;
    uint32_t    count;

private:

    friend class VulkanDevice;

    IndexBuffer(VulkanDevice *owner,VkDevice d,const DeviceBufferData &vb,IndexType it,uint32_t _count):DeviceBuffer(owner,d,vb)
    {
        index_type=it;
        count=_count;

        if(index_type==IndexType::U16)stride=2;else
        if(index_type==IndexType::U32)stride=4;else
        if(index_type==IndexType::U8)stride=1;else
            stride=0;
    }

    IndexBuffer(VulkanDevice *owner,VkDevice d,const DeviceBufferData &vb,IndexType it,uint32_t _count,StagedBuffer *sb):DeviceBuffer(owner,d,vb,sb)
    {
        index_type=it;
        count=_count;

        if(index_type==IndexType::U16)stride=2;else
        if(index_type==IndexType::U32)stride=4;else
        if(index_type==IndexType::U8)stride=1;else
            stride=0;
    }

public:

    ~IndexBuffer()=default;

    const IndexType     GetType     ()const{return index_type;}
    const uint          GetStride   ()const{return stride;}
    const uint32        GetCount    ()const{return count;}

public:

    void *  Map     (VkDeviceSize start,VkDeviceSize size)          override {return DeviceBuffer::Map(start*stride,size*stride);}
    void    Flush   (VkDeviceSize start,VkDeviceSize size)          override {return DeviceBuffer::Flush(start*stride,size*stride); }
    void    Flush   (VkDeviceSize size)                             override {return DeviceBuffer::Flush(size*stride);}

    bool    Write   (const void *ptr,uint32_t start,uint32_t size)  override {return DeviceBuffer::Write(ptr,start*stride,size*stride);}
    bool    Write   (const void *ptr,uint32_t size)                 override {return DeviceBuffer::Write(ptr,0,size*stride);}
};//class IndexBuffer:public DeviceBuffer

}//namespace hgl::graph
