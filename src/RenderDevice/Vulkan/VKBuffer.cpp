#include<hgl/graph/vulkan/VKBuffer.h>

VK_NAMESPACE_BEGIN
Buffer::~Buffer()
{
    vkFreeMemory(device,buf.memory,nullptr);
    vkDestroyBuffer(device,buf.buffer,nullptr);
}

uint8_t *Buffer::Map(uint32_t start,uint32_t size)
{
    uint8_t *p;

    if(start>buf.info.range
     ||start+size>buf.info.range)
        return(nullptr);

    if(start==0&&size==0)
        size=buf.info.range;

    if(vkMapMemory(device,buf.memory,start,size,0,(void **)&p)==VK_SUCCESS)
        return p;

    return nullptr;
}
void Buffer::Unmap()
{
    vkUnmapMemory(device,buf.memory);
}

bool Buffer::Write(const void *ptr,uint32_t start,uint32_t size)
{
    if(!ptr)return(false);

    void *dst;

    if(vkMapMemory(device,buf.memory,start,size,0,&dst)!=VK_SUCCESS)
        return(false);

    memcpy(dst,ptr,size);
    vkUnmapMemory(device,buf.memory);
    return(true);
}
VK_NAMESPACE_END
