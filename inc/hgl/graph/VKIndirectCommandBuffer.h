#pragma once

#include<hgl/graph/VKBuffer.h>

VK_NAMESPACE_BEGIN

class IndirectCommandBuffer:public DeviceBuffer
{
protected:

    uint32_t max_count;
    uint32_t stride;

private:

    friend class GPUDevice;

    IndirectCommandBuffer(VkDevice d,const DeviceBufferData &vb,const uint32_t mc,const uint32_t s):DeviceBuffer(d,vb)
    {
        max_count=mc;
        stride=s;
    }

    ~IndirectCommandBuffer()=default;

    const uint32_t GetMaxCount ()const { return max_count; }

    const VkDeviceSize GetTotalBytes()const { return stride*max_count; }
    
    void *  Map     (VkDeviceSize start,VkDeviceSize size)          override {return DeviceBuffer::Map(start*stride,size*stride);}
    void    Flush   (VkDeviceSize start,VkDeviceSize size)          override {return DeviceBuffer::Flush(start*stride,size*stride);}
    void    Flush   (VkDeviceSize size)                             override {return DeviceBuffer::Flush(size*stride);}

    bool    Write   (const void *ptr,uint32_t start,uint32_t size)  override {return DeviceBuffer::Write(ptr,start*stride,size*stride);}
    bool    Write   (const void *ptr,uint32_t size)                 override {return DeviceBuffer::Write(ptr,0,size*stride);}
};//class IndirectCommandBuffer:public DeviceBuffer
VK_NAMESPACE_END
