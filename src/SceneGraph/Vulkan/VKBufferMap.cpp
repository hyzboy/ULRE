#include<hgl/graph/VKBufferMap.h>
#include<hgl/graph/VKVertexAttribBuffer.h>

VK_NAMESPACE_BEGIN

VKBufferMap::VKBufferMap(DeviceBuffer *buf,VkDeviceSize off,VkDeviceSize s)
{
    buffer=buf;
    offset=off;
    size=s;

    map_ptr=nullptr;
}

VKBufferMap::~VKBufferMap()
{
    if(map_ptr)
        buffer->DeviceBuffer::Unmap();
}

void *VKBufferMap::Map()
{
    if(map_ptr)
        return(map_ptr);

    if(!buffer)
        return(nullptr);

    map_ptr=buffer->DeviceBuffer::Map(offset,size);
    return map_ptr;

}

void VKBufferMap::Unmap()
{
    if(map_ptr)
    {
        buffer->DeviceBuffer::Unmap();
        map_ptr=nullptr;
    }
}

VK_NAMESPACE_END