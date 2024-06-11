#pragma once

#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKBufferMap.h>

VK_NAMESPACE_BEGIN
class IndexBuffer:public DeviceBuffer
{
    IndexType   index_type;
    uint        stride;
    uint32_t    count;

private:

    friend class GPUDevice;

    IndexBuffer(VkDevice d,const DeviceBufferData &vb,IndexType it,uint32_t _count):DeviceBuffer(d,vb)
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

class IBMap:public VKBufferMap<IndexBuffer>
{
public:

    using VKBufferMap<IndexBuffer>::VKBufferMap;
    virtual ~IBMap()=default;

    const IndexType GetType()const{return buffer->GetType();}

    void SetIBO(IndexBuffer *ib,const int32_t index_offset,const uint32_t count)
    {
        VKBufferMap<IndexBuffer>::Set(ib,index_offset,ib->GetStride(),count);
    }
};//class IBMap

/**
* 索引缓冲区数据访问映射
*/
template<typename T> class IBTypeMap
{
    IBMap *ib_map;

    T *map_ptr;

public:

    IBTypeMap(IBMap *ibm)
    {   
        ib_map=ibm;

        if(ib_map&&ib_map->GetStride()==sizeof(T))
            map_ptr=(T *)(ib_map->Map());
        else
            map_ptr=nullptr;
    }

    ~IBTypeMap()
    {
        if(map_ptr)
            ib_map->Unmap();
    }

    const bool IsValid()const{ return map_ptr; }

    operator T *(){ return map_ptr; }
    T *operator->(){ return map_ptr; }
};//template<typename T> class IBTypeMap

using IBMapU8 =IBTypeMap<uint8>;
using IBMapU16=IBTypeMap<uint16>;
using IBMapU32=IBTypeMap<uint32>;

VK_NAMESPACE_END
