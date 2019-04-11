#include"VKBuffer.h"

VK_NAMESPACE_BEGIN
Buffer::~Buffer()
{
    vkDestroyBuffer(device,buf,nullptr);
    vkFreeMemory(device,mem,nullptr);
}

uint8_t *Buffer::Map(uint32_t start,uint32_t size)
{
    uint8_t *p;

    if(start>buffer_info.range
     ||start+size>buffer_info.range)
        return(nullptr);

    if(start==0&&size==0)
        size=buffer_info.range;

    if(vkMapMemory(device,mem,start,size,0,(void **)&p)==VK_SUCCESS)
        return p;

    return nullptr;
}
void Buffer::Unmap()
{
    vkUnmapMemory(device,mem);
}
VK_NAMESPACE_END
