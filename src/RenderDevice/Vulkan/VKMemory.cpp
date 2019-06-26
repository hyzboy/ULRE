#include<hgl/graph/vulkan/VKMemory.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>
VK_NAMESPACE_BEGIN
Memory *CreateMemory(VkDevice device,const PhysicalDevice *pd,const VkMemoryRequirements &req,uint32_t properties)
{
    uint32_t index;

    if(!pd->CheckMemoryType(req.memoryTypeBits,properties,&index))
        return(false);

    VkMemoryAllocateInfo alloc_info;

    alloc_info.sType            =VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext            =nullptr;
    alloc_info.memoryTypeIndex  =index;
    alloc_info.allocationSize   =req.size;

    VkDeviceMemory memory;

    if(vkAllocateMemory(device,&alloc_info,nullptr,&memory)!=VK_SUCCESS)
        return(nullptr);

    return(new Memory(device,memory,req,index,properties));
}

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

bool Memory::Write(const void *ptr,VkDeviceSize start,VkDeviceSize size)
{
    if(!ptr)return(false);

    void *dst;

    if(vkMapMemory(device,memory,start,size,0,&dst)!=VK_SUCCESS)
        return(false);

    memcpy(dst,ptr,size);
    vkUnmapMemory(device,memory);
    return(true);
}

bool Memory::Bind(VkBuffer buffer)
{
    if(!buffer)return(false);

    return(vkBindBufferMemory(device,buffer,memory,0)==VK_SUCCESS);
}

bool Memory::Bind(VkImage image)
{
    if(!image)return(false);

    return(vkBindImageMemory(device,image,memory,0)==VK_SUCCESS);
}
VK_NAMESPACE_END
