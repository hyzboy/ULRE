#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKImageCreateInfo.h>

VK_NAMESPACE_BEGIN
VkImage GPUDevice::CreateImage(VkImageCreateInfo *ici)
{
    if(!ici)return(VK_NULL_HANDLE);
    if(!CheckVulkanFormat(ici->format))return(VK_NULL_HANDLE);
    if(ici->extent.width*ici->extent.height*ici->extent.depth*ici->arrayLayers<=0)return(VK_NULL_HANDLE);

    VkImage image;

    if(vkCreateImage(attr->device,ici, nullptr, &image)!=VK_SUCCESS)
        return(nullptr);

    return image;
}

void GPUDevice::DestoryImage(VkImage img)
{
    if(img==VK_NULL_HANDLE)return;

    vkDestroyImage(attr->device,img,nullptr);
}

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
