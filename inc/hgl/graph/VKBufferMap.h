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
};//class VKBufferMap
VK_NAMESPACE_END
