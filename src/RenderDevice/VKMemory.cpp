#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMemory.h>
#include<hgl/graph/VKPhysicalDevice.h>
VK_NAMESPACE_BEGIN
GPUMemory *RenderDevice::CreateMemory(const VkMemoryRequirements &req,uint32_t properties)
{
    uint32_t index;

    if(!attr->physical_device->CheckMemoryType(req.memoryTypeBits,properties,&index))
        return(nullptr);

    VkMemoryAllocateInfo alloc_info;

    alloc_info.sType            =VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext            =nullptr;
    alloc_info.memoryTypeIndex  =index;
    alloc_info.allocationSize   =req.size;

    VkDeviceMemory memory;

    if(vkAllocateMemory(attr->device,&alloc_info,nullptr,&memory)!=VK_SUCCESS)
        return(nullptr);

    return(new GPUMemory(attr->device,memory,req,index,properties));
}

GPUMemory::~GPUMemory()
{
    vkFreeMemory(device,memory,nullptr);
}

void *GPUMemory::Map()
{
    void *result;

    if(vkMapMemory(device,memory,0,req.size,0,&result)==VK_SUCCESS)
        return result;

    return(nullptr);
}

void *GPUMemory::Map(const VkDeviceSize offset,const VkDeviceSize size)
{
    if(offset<0||offset+size>=req.size)
        return(nullptr);

    void *result;

    if(vkMapMemory(device,memory,0,size,0,&result)==VK_SUCCESS)
        return result;

    return(nullptr);
}

void GPUMemory::Unmap()
{
    vkUnmapMemory(device,memory);
}

bool GPUMemory::Write(const void *ptr,VkDeviceSize start,VkDeviceSize size)
{
    if(!ptr)return(false);

    void *dst;

    if(vkMapMemory(device,memory,start,size,0,&dst)!=VK_SUCCESS)
        return(false);

    memcpy(dst,ptr,size);
    vkUnmapMemory(device,memory);
    return(true);
}

bool GPUMemory::BindBuffer(VkBuffer buffer)
{
    if(!buffer)return(false);

    return(vkBindBufferMemory(device,buffer,memory,0)==VK_SUCCESS);
}

bool GPUMemory::BindImage(VkImage image)
{
    if(!image)return(false);

    return(vkBindImageMemory(device,image,memory,0)==VK_SUCCESS);
}
VK_NAMESPACE_END
