#include"VKBuffer.h"

VK_NAMESPACE_BEGIN
Buffer::~Buffer()
{
    vkDestroyBuffer(device,buf,nullptr);
    vkFreeMemory(device,mem,nullptr);
}

uint8_t *Buffer::Map()
{
    uint8_t *p;

    if(vkMapMemory(device,mem,0,buffer_info.range,0,(void **)&p)==VK_SUCCESS)
        return p;

    return nullptr;
}
void Buffer::Unmap()
{
    vkUnmapMemory(device,mem);
}
VK_NAMESPACE_END
