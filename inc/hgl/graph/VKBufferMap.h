#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VertexAttribDataAccess.h>

VK_NAMESPACE_BEGIN
class VKBufferMap
{
protected:

    DeviceBuffer *buffer;
    VkDeviceSize offset;
    VkDeviceSize size;

    void *map_ptr;

public:

    VKBufferMap(DeviceBuffer *buf_ptr,VkDeviceSize off,VkDeviceSize s);
    virtual ~VKBufferMap();

    const bool IsValid()const{ return buffer; }

    void *Map();
    void Unmap();
};//class VKBufferMap
VK_NAMESPACE_END
