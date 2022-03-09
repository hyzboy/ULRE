#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMemory.h>
#include<hgl/graph/VKPhysicalDevice.h>
VK_NAMESPACE_BEGIN
GPUMemory *GPUDevice::CreateMemory(const VkMemoryRequirements &req,uint32_t properties)
{
    const int index=attr->physical_device->GetMemoryType(req.memoryTypeBits,properties);

    if(index<0)
        return(nullptr);

    MemoryAllocateInfo alloc_info(index,req.size);

    VkDeviceMemory memory;

    if(vkAllocateMemory(attr->device,&alloc_info,nullptr,&memory)!=VK_SUCCESS)
        return(nullptr);

    return(new GPUMemory(attr->device,memory,req,index,properties,attr->physical_device->GetLimits().nonCoherentAtomSize));
}

GPUMemory::GPUMemory(VkDevice dev,VkDeviceMemory dm,const VkMemoryRequirements &mr,const uint32 i,const uint32_t p,const VkDeviceSize cas)
{
    device=dev;
    memory=dm;
    req=mr;
    index=i;
    properties=p;

    memory_range.sType  =VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    memory_range.pNext  =nullptr;
    memory_range.memory =memory;

    nonCoherentAtomSize=cas;
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

    if(vkMapMemory(device,memory,offset,size,0,&result)==VK_SUCCESS)
        return result;

    return(nullptr);
}

void GPUMemory::Unmap()
{
    vkUnmapMemory(device,memory);
}

void GPUMemory::Flush(VkDeviceSize offset,VkDeviceSize size)
{
    memory_range.offset =offset;
    memory_range.size   =size>0?hgl_align(size,nonCoherentAtomSize):0;

    vkFlushMappedMemoryRanges(device,1,&memory_range);
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
