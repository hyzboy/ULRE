#include<hgl/graph/VKBuffer.h>

VK_NAMESPACE_BEGIN
DeviceBuffer::~DeviceBuffer()
{
    if(staged_buffer)
    {
        delete staged_buffer;
        staged_buffer=nullptr;
        buf.memory=nullptr;
        buf.buffer=nullptr;
        return;
    }

    if(buf.memory)delete buf.memory;
    if(buf.buffer)vkDestroyBuffer(device,buf.buffer,nullptr);
}
VK_NAMESPACE_END
