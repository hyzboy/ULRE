#include<hgl/graph/VKBuffer.h>

VK_NAMESPACE_BEGIN
GPUBuffer::~GPUBuffer()
{
    if(buf.memory)delete buf.memory;
    vkDestroyBuffer(device,buf.buffer,nullptr);
}
VK_NAMESPACE_END
