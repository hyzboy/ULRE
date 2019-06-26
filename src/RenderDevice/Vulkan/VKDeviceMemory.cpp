#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKMemory.h>
VK_NAMESPACE_BEGIN
Memory *Device::CreateMemory(const VkMemoryRequirements &req,uint32_t properties)
{
    uint32_t index;

    if(!attr->CheckMemoryType(req.memoryTypeBits,properties,&index))
        return(false);

    VkMemoryAllocateInfo alloc_info;

    alloc_info.sType            =VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext            =nullptr;
    alloc_info.memoryTypeIndex  =index;
    alloc_info.allocationSize   =req.size;

    VkDeviceMemory memory;

    if(vkAllocateMemory(attr->device,&alloc_info,nullptr,&memory)!=VK_SUCCESS)
        return(nullptr);

    return(new Memory(attr->device,memory,req,index,properties));
}
VK_NAMESPACE_END
