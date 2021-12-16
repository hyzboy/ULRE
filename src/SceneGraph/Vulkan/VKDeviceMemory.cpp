#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN
GPUMemory *GPUDevice::CreateMemory(VkImage image,const uint32_t flag)
{
    VkMemoryRequirements memReqs;

    vkGetImageMemoryRequirements(attr->device,image,&memReqs);

    GPUMemory *mem=CreateMemory(memReqs,flag);
    
    if(!mem)return(nullptr);

    if(!mem->BindImage(image))
    {
        delete mem;
        return(nullptr);
    }

    return(mem);
}
VK_NAMESPACE_END
