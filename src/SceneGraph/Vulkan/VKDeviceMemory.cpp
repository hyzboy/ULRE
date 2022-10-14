#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN
DeviceMemory *GPUDevice::CreateMemory(VkImage image,const uint32_t flag)
{
    VkMemoryRequirements memReqs;

    vkGetImageMemoryRequirements(attr->device,image,&memReqs);

    DeviceMemory *mem=CreateMemory(memReqs,flag);
    
    if(!mem)return(nullptr);

    if(!mem->BindImage(image))
    {
        delete mem;
        return(nullptr);
    }

    return(mem);
}
VK_NAMESPACE_END
