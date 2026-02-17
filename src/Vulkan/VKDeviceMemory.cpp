#include<hgl/vk/VKDevice.h>

namespace hgl::graph{
DeviceMemory *VulkanDevice::CreateMemory(VkImage image,const uint32_t flag, const ObjectNameBuilder &name, const std::source_location &loc)
{
    VkMemoryRequirements memReqs;

    vkGetImageMemoryRequirements(attr->device,image,&memReqs);

    DeviceMemory *mem=CreateMemory(memReqs,flag,name,loc);

    if(!mem)return(nullptr);

    if(!mem->BindImage(image))
    {
        delete mem;
        return(nullptr);
    }

    return(mem);
}
}//namespace hgl::graph
