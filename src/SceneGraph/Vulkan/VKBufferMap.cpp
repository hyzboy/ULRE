#include<hgl/graph/VKBufferMap.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<iostream>

VK_NAMESPACE_BEGIN

void VKBufferMap::Set(DeviceBuffer *buf,VkDeviceSize off,VkDeviceSize s)
{    
    buffer=buf;
    offset=off;
    size=s;

    map_ptr=nullptr;
}

void VKBufferMap::Clear()
{
    if(buffer&&map_ptr)
        buffer->Unmap();

    buffer=nullptr;
    offset=0;
    size=0;
    map_ptr=nullptr;
}

VKBufferMap::VKBufferMap()
{
    Set(nullptr,0,0);
    
    std::cout<<"VKBufferMap Create"<<std::endl;
}

VKBufferMap::VKBufferMap(DeviceBuffer *buf,VkDeviceSize off,VkDeviceSize s)
{
    Set(buf,off,s);
}

VKBufferMap::~VKBufferMap()
{
    if(buffer&&map_ptr)
        buffer->Unmap();

    std::cout<<"VKBufferMap Destory"<<std::endl;
}

void *VKBufferMap::Map()
{
    if(map_ptr)
        return(map_ptr);

    if(!buffer)
        return(nullptr);

    map_ptr=buffer->Map(offset,size);
    return map_ptr;

}

void VKBufferMap::Unmap()
{
    if(buffer&&map_ptr)
    {
        buffer->Unmap();
        map_ptr=nullptr;
    }
}

VK_NAMESPACE_END
