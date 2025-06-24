#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMemory.h>
#include<hgl/graph/VKPhysicalDevice.h>
VK_NAMESPACE_BEGIN
DeviceMemory *VulkanDevice::CreateMemory(const VkMemoryRequirements &req,uint32_t properties)
{
    const int index=attr->physical_device->GetMemoryType(req.memoryTypeBits,properties);

    if(index<0)
        return(nullptr);

    MemoryAllocateInfo alloc_info(index,req.size);

    VkDeviceMemory memory;

    if(vkAllocateMemory(attr->device,&alloc_info,nullptr,&memory)!=VK_SUCCESS)
        return(nullptr);

    return(new DeviceMemory(attr->device,memory,req,index,properties,attr->physical_device->GetLimits().nonCoherentAtomSize));
}

DeviceMemory::DeviceMemory(VkDevice dev,VkDeviceMemory dm,const VkMemoryRequirements &mr,const uint32 i,const uint32_t p,const VkDeviceSize cas)
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

DeviceMemory::~DeviceMemory()
{
    vkFreeMemory(device,memory,nullptr);
}

void *DeviceMemory::Map()
{
    void *result;

    if(vkMapMemory(device,memory,0,req.size,0,&result)==VK_SUCCESS)
    {
        /** 只要是MAP成功，那么数据就可以直接访问

        关键点
        1.	如果你的内存类型是 HOST_VISIBLE 且 HOST_COHERENT
            •	这类内存通常是“共享内存”（integrated GPU）或“staging buffer”（独立显卡+主机内存）。
            •	读取和写入都比较高效，类似普通RAM，但带宽和延迟可能略低于纯CPU内存。
            •	适合CPU直接读写。
        2.	如果你的内存类型是 HOST_VISIBLE 但不是 HOST_COHERENT
            •	你在写入后需要vkFlushMappedMemoryRanges，读取前需要vkInvalidateMappedMemoryRanges。
            •	否则CPU和GPU看到的数据可能不同步。
            •	读取时要注意同步，否则可能读到旧数据。
        3.	如果你的内存类型是纯GPU本地内存（DEVICE_LOCAL，无HOST_VISIBLE）
            •	你根本无法vkMapMemory，也无法直接读取。

        性能建议
        •	遍历读取通常比写入慢，因为数据可能在主板内存（staging buffer），而不是CPU缓存友好的区域。
        •	如果只是偶尔小范围读取，影响不大。
        •	如果频繁大范围遍历，建议：
        •	尽量减少读取次数和数据量。
        •	考虑在上传前先在CPU内存准备好数据，必要时用临时buffer。
        •	如果数据已经上传到GPU且只在GPU用，避免CPU端读取。
        典型用法
        •	上传数据：CPU写入staging buffer → flush → GPU拷贝到本地显存。
        •	读取数据：很少直接从映射内存读取，通常是GPU拷贝到staging buffer，再由CPU读取。
        总结
        •	偶尔小量读取没问题，大范围频繁读取会有性能损失。
        •	建议只在必要时读取，并优先考虑数据在CPU端准备好后一次性上传。
        */

        return result;
    }

    return(nullptr);
}

void *DeviceMemory::Map(const VkDeviceSize offset,const VkDeviceSize size)
{
    if(offset<0||offset+size>req.size)
        return(nullptr);

    void *result;

    if(vkMapMemory(device,memory,offset,size,0,&result)==VK_SUCCESS)
        return result;

    return(nullptr);
}

void DeviceMemory::Unmap()
{
    vkUnmapMemory(device,memory);
}

void DeviceMemory::Flush(VkDeviceSize offset,VkDeviceSize size)
{
    memory_range.offset =offset;
    memory_range.size   =size>0?hgl_align(size,nonCoherentAtomSize):0;

    vkFlushMappedMemoryRanges(device,1,&memory_range);
}

bool DeviceMemory::Write(const void *ptr,VkDeviceSize start,VkDeviceSize size)
{
    if(!ptr)return(false);

    void *dst;

    if(vkMapMemory(device,memory,start,size,0,&dst)!=VK_SUCCESS)
        return(false);

    memcpy(dst,ptr,size);
    vkUnmapMemory(device,memory);
    return(true);
}

bool DeviceMemory::BindBuffer(VkBuffer buffer)
{
    if(!buffer)return(false);

    return(vkBindBufferMemory(device,buffer,memory,0)==VK_SUCCESS);
}

bool DeviceMemory::BindImage(VkImage image)
{
    if(!image)return(false);

    return(vkBindImageMemory(device,image,memory,0)==VK_SUCCESS);
}
VK_NAMESPACE_END
