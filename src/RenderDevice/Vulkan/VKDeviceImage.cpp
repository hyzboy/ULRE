#include<hgl/graph/vulkan/VKDevice.h>

VK_NAMESPACE_BEGIN
VkImage Device::CreateImage(const VkFormat format,uint32_t width,uint32_t height,const uint usage,const VkImageTiling tiling)
{
    if(!CheckVulkanFormat(format))return(nullptr);
    if(width<1||height<1)return(nullptr);

    VkImageCreateInfo imageCreateInfo;

    imageCreateInfo.sType                   = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext                   = nullptr;
    imageCreateInfo.flags                   = 0;
    imageCreateInfo.imageType               = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format                  = format;
    imageCreateInfo.extent.width            = width;
    imageCreateInfo.extent.height           = height;
    imageCreateInfo.extent.depth            = 1;
    imageCreateInfo.mipLevels               = 1;
    imageCreateInfo.arrayLayers             = 1;
    imageCreateInfo.samples                 = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.usage                   = usage;
    imageCreateInfo.sharingMode             = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount   = 0;
    imageCreateInfo.pQueueFamilyIndices     = nullptr;
    imageCreateInfo.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.tiling                  = tiling;

    VkImage image;

    if(vkCreateImage(attr->device,&imageCreateInfo, nullptr, &image)!=VK_SUCCESS)
        return(nullptr);

    return image;
}

void Device::DestoryImage(VkImage img)
{
    if(img==VK_NULL_HANDLE)return;

    vkDestroyImage(attr->device,img,nullptr);
}

Memory *Device::CreateMemory(VkImage image,const uint32_t flag)
{
    VkMemoryRequirements memReqs;

    vkGetImageMemoryRequirements(attr->device,image,&memReqs);

    return CreateMemory(memReqs,flag);
}
VK_NAMESPACE_END
