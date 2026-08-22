#pragma once

#include<hgl/vk/VKBufferOwner.h>

namespace hgl::graph{
class IndexBuffer:public VkBufferOwner
{
    IndexType   index_type;
    uint        stride;
    uint32_t    count;

private:

    friend class VulkanDevice;

    IndexBuffer(VkDevice d,const DeviceBufferData &vb,IndexType it,uint32_t _count):VkBufferOwner(d,vb)
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

    void *  Map     (VkDeviceSize start,VkDeviceSize size)          {return GetGPUBuffer()->Map(start*stride,size*stride);}
    void    Unmap   ()                                              {GetGPUBuffer()->Unmap();}
    void    Flush   (VkDeviceSize start,VkDeviceSize size)          {GetGPUBuffer()->MarkDirty(start*stride,size*stride);}
    void    Flush   (VkDeviceSize size)                             {GetGPUBuffer()->MarkDirty(0,size*stride);}

    bool    Write   (const void *ptr,uint32_t start,uint32_t size)  {return GetGPUBuffer()->Write(ptr,start*stride,size*stride);}
    bool    Write   (const void *ptr,uint32_t size)                 {return GetGPUBuffer()->Write(ptr,0,size*stride);}

    /**
     * Returns the underlying VkBuffer handle for SSBO 顶点索引绑定（sbo_index 查表路径）。
     * Prefer this over GetBuffer() — does not require DeviceBuffer inheritance.
     */
    VkBuffer GetVkBuffer() const { return GetGPUBuffer()->GetVkDeviceBuffer(); }
};//class IndexBuffer:public VkBufferOwner

}//namespace hgl::graph
