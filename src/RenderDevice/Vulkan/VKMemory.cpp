#include<hgl/graph/vulkan/VKMemory.h>
VK_NAMESPACE_BEGIN
Memory::~Memory()
{
    vkFreeMemory(device,memory,nullptr);
}

void *Memory::Map()
{
    void *result;

    if(vkMapMemory(device,memory,0,req.size,0,&result)==VK_SUCCESS)
        return result;

    return(nullptr);
}

void *Memory::Map(const VkDeviceSize offset,const VkDeviceSize size)
{
    if(offset<0||offset+size>=req.size)
        return(nullptr);

    void *result;

    if(vkMapMemory(device,memory,0,size,0,&result)==VK_SUCCESS)
        return result;

    return(nullptr);
}

void Memory::Unmap()
{
    vkUnmapMemory(device,memory);
}

bool Memory::Bind(VkImage image)
{
    if(!image)return(false);

    return(vkBindImageMemory(device,image,memory,0)==VK_SUCCESS);
}
VK_NAMESPACE_END
