#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VertexAttribDataAccess.h>

VK_NAMESPACE_BEGIN
template<typename T> class VKBufferMap
{
protected:

    T *buffer;
    int32_t offset;
    uint32_t stride;
    uint32_t count;

    void *map_ptr;

public:

    VKBufferMap()
    {
        buffer=nullptr;
        offset=0;
        stride=count=0;
    }

    virtual ~VKBufferMap()
    {
        Unmap();
    }

    void Set(T *buf,const int32_t off,const uint32_t s,const uint32_t c)
    {
        buffer=buf;
        offset=off;
        stride=s;
        count=c;

        map_ptr=nullptr;
    }

    const int32_t   GetOffset  ()const{ return offset;}
    const uint32_t  GetStride  ()const{ return stride;}
    const uint32_t  GetCount   ()const{ return count; }

    const bool IsValid()const{ return buffer; }

    void Clear()
    {
        Unmap();

        buffer=nullptr;
        offset=0;
        stride=count=0;
    }

    void *Map()
    {
        if(map_ptr)
            return(map_ptr);

        if(!buffer)
            return(nullptr);

        map_ptr=buffer->Map(offset,count);
        return map_ptr;
    }

    void Unmap()
    {    
        if(buffer&&map_ptr)
        {
            buffer->Unmap();
            map_ptr=nullptr;
        }
    }

    bool Write(const void *data,const uint32_t c)
    {
        if(!data||c==0||c>count)return(false);

        if(!map_ptr)
        {
            if(!buffer)
                return(false);

            /*
            * 注：这里的不管是offset还是c，都会走VAB/IndexBuffer的虚拟版本。
            *    其中的数据均为单元数据长度而非字节数。
            * 
            *    如果需要传字节数，就需要buffer->DeviceBuffer::Write()这样操作
            */
            return buffer->Write(data,offset,c);
        }

        memcpy(map_ptr,data,stride*c);
        return(true);
    }
};//class VKBufferMap
VK_NAMESPACE_END
