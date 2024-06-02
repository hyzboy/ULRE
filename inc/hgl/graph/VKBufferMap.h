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

    VKBufferMap();
    VKBufferMap(DeviceBuffer *buf,VkDeviceSize off,VkDeviceSize s);
    ~VKBufferMap();

    void Set(DeviceBuffer *buf_ptr,VkDeviceSize off,VkDeviceSize s);

    const bool IsValid()const{ return buffer; }
    void Clear();

    void *Map();
    void Unmap();
};//class VKBufferMap
VK_NAMESPACE_END
