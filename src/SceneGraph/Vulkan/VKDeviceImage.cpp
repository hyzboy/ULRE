#include<hgl/graph/manager/TextureManager.h>
#include<hgl/graph/VKImageCreateInfo.h>

VK_NAMESPACE_BEGIN
VkImage TextureManager::CreateImage(VkImageCreateInfo *ici)
{
    if(!ici)return(VK_NULL_HANDLE);
    if(!CheckVulkanFormat(ici->format))return(VK_NULL_HANDLE);
    if(ici->extent.width*ici->extent.height*ici->extent.depth*ici->arrayLayers<=0)return(VK_NULL_HANDLE);

    VkImage image;

    if(vkCreateImage(GetVkDevice(),ici, nullptr, &image)!=VK_SUCCESS)
        return(nullptr);

    return image;
}

void TextureManager::DestroyImage(VkImage img)
{
    if(img==VK_NULL_HANDLE)return;

    vkDestroyImage(GetVkDevice(),img,nullptr);
}
VK_NAMESPACE_END
