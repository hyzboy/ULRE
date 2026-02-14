#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKMemory.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/log/Log.h>
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
    is_mapped=false;  // Initially not mapped
    mapped_ptr=nullptr; // No mapped pointer initially

    memory_range.sType  =VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    memory_range.pNext  =nullptr;
    memory_range.memory =memory;

    nonCoherentAtomSize=cas;
}

DeviceMemory::~DeviceMemory()
{
    if(is_mapped)
    {
        vkUnmapMemory(device,memory);
        is_mapped=false;
    }

    vkFreeMemory(device,memory,nullptr);
}

void *DeviceMemory::Map()
{
    if(is_mapped)
    {
        // Already mapped, return existing pointer
        return mapped_ptr;
    }

    void *result;

    if(vkMapMemory(device,memory,0,req.size,0,&result)==VK_SUCCESS)
    {
        is_mapped=true;
        mapped_ptr=result;
        mapped_offset = 0;
        mapped_size = req.size;

        /** 只要是MAP成功，那么数据就可以直接访问

        关键点
        1.  如果你的内存类型是 HOST_VISIBLE 且 HOST_COHERENT
            ?   这类内存通常是“共享内存”（integrated GPU）或“staging buffer”（独立显卡+主机内存）。
            ?   读取和写入都比较高效，类似普通RAM，但带宽和延迟可能略低于纯CPU内存。
            ?   适合CPU直接读写。
        2.  如果你的内存类型是 HOST_VISIBLE 但不是 HOST_COHERENT
            ?   你在写入后需要vkFlushMappedMemoryRanges，读取前需要vkInvalidateMappedMemoryRanges。
            ?   否则CPU和GPU看到的数据可能不同步。
            ?   读取时要注意同步，否则可能读到旧数据。
        3.  如果你的内存类型是纯GPU本地内存（DEVICE_LOCAL，无HOST_VISIBLE）
            ?   你根本无法vkMapMemory，也无法直接读取。

        性能建议
        ?   遍历读取通常比写入慢，因为数据可能在主板内存（staging buffer），而不是CPU缓存友好的区域。
        ?   如果只是偶尔小范围读取，影响不大。
        ?   如果频繁大范围遍历，建议：
        ?   尽量减少读取次数和数据量。
        ?   考虑在上传前先在CPU内存准备好数据，必要时用临时buffer。
        ?   如果数据已经上传到GPU且只在GPU用，避免CPU端读取。
        典型用法
        ?   上传数据：CPU写入staging buffer → flush → GPU拷贝到本地显存。
        ?   读取数据：很少直接从映射内存读取，通常是GPU拷贝到staging buffer，再由CPU读取。
        总结
        ?   偶尔小量读取没问题，大范围频繁读取会有性能损失。
        ?   建议只在必要时读取，并优先考虑数据在CPU端准备好后一次性上传。
        */

        return mapped_ptr;
    }

    return(nullptr);
}

void *DeviceMemory::Map(const VkDeviceSize offset,const VkDeviceSize size)
{
    if(offset<0||offset+size>req.size)
        return(nullptr);

    if(is_mapped)
    {
        // Already mapped, return pointer with offset adjustment
        // Note: mapped_ptr points to the start of the mapped region (offset 0)
        return (char*)mapped_ptr + offset;
    }

    // Always map the entire memory to avoid partial mapping conflicts
    void *result;

    if(vkMapMemory(device,memory,0,req.size,0,&result)==VK_SUCCESS)
    {
        is_mapped=true;
        mapped_ptr=result;
        mapped_offset = 0;
        mapped_size = req.size;
        // Return pointer with requested offset
        return (char*)result + offset;
    }

    return(nullptr);
}

void DeviceMemory::Unmap()
{
    if(!is_mapped)
    {
        // Not currently mapped, skip unmap to avoid validation error
        return;
    }

    vkUnmapMemory(device,memory);
    is_mapped=false;
    mapped_ptr=nullptr;
    mapped_offset = 0;
    mapped_size = 0;
}

void DeviceMemory::Flush(VkDeviceSize offset,VkDeviceSize size)
{
    if(!is_mapped)
    {
        // Not currently mapped, skip flush to avoid validation error
        return;
    }

    VkDeviceSize aligned_size = size;
    if(size == VK_WHOLE_SIZE)
        aligned_size = VK_WHOLE_SIZE;
    else if(size > 0)
        aligned_size = hgl_align(size, nonCoherentAtomSize);

    if(size == VK_WHOLE_SIZE)
    {
        if(offset < mapped_offset || offset >= mapped_offset + mapped_size)
        {
            GLogWarning("[DeviceMemory::Flush] whole-size offset out of mapped range memory=%p vkMemory=%p offset=%llu mappedOffset=%llu mappedSize=%llu",
                        (void *)this,
                        (void *)memory,
                        static_cast<unsigned long long>(offset),
                        static_cast<unsigned long long>(mapped_offset),
                        static_cast<unsigned long long>(mapped_size));
            return;
        }
    }
    else
    {
        const VkDeviceSize mapped_end = mapped_offset + mapped_size;
        if(offset < mapped_offset || offset + aligned_size > mapped_end)
        {
            GLogWarning("[DeviceMemory::Flush] range exceeds mapped region memory=%p vkMemory=%p offset=%llu size=%llu aligned=%llu mappedOffset=%llu mappedSize=%llu",
                        (void *)this,
                        (void *)memory,
                        static_cast<unsigned long long>(offset),
                        static_cast<unsigned long long>(size),
                        static_cast<unsigned long long>(aligned_size),
                        static_cast<unsigned long long>(mapped_offset),
                        static_cast<unsigned long long>(mapped_size));
            return;
        }
    }

    if(size != VK_WHOLE_SIZE && aligned_size > 0 && offset + aligned_size > req.size)
    {
        GLogWarning("[DeviceMemory::Flush] range overflow memory=%p vkMemory=%p offset=%llu size=%llu aligned=%llu memSize=%llu atomSize=%llu",
                    (void *)this,
                    (void *)memory,
                    static_cast<unsigned long long>(offset),
                    static_cast<unsigned long long>(size),
                    static_cast<unsigned long long>(aligned_size),
                    static_cast<unsigned long long>(req.size),
                    static_cast<unsigned long long>(nonCoherentAtomSize));
    }

    memory_range.offset = offset;
    memory_range.size   = aligned_size;

    vkFlushMappedMemoryRanges(device,1,&memory_range);
}

bool DeviceMemory::Write(const void *ptr,VkDeviceSize start,VkDeviceSize size)
{
    if(!ptr)return(false);

    // Check if already mapped - use existing mapping
    if(is_mapped)
    {
        if(!mapped_ptr)return false;
        // Use existing mapped pointer (assuming offset matches)
        memcpy((char*)mapped_ptr+start,ptr,size);
        return true;
    }

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
