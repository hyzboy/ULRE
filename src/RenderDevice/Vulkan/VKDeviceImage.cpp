#include<hgl/graph/vulkan/VKDevice.h>

VK_NAMESPACE_BEGIN
namespace
{
    void InitImageCreateInfo(VkImageCreateInfo &imageCreateInfo)
    {
        imageCreateInfo.sType                   = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.pNext                   = nullptr;
        imageCreateInfo.flags                   = 0;
        imageCreateInfo.mipLevels               = 1;
        imageCreateInfo.arrayLayers             = 1;
        imageCreateInfo.samples                 = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.sharingMode             = VkSharingMode(SharingMode::Exclusive);
        imageCreateInfo.queueFamilyIndexCount   = 0;
        imageCreateInfo.pQueueFamilyIndices     = nullptr;
        imageCreateInfo.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    }
}//namespace

VkImage Device::CreateImage1D(const VkFormat format,const uint32_t length,const uint usage,const ImageTiling tiling)
{
    if(!CheckVulkanFormat(format))return(nullptr);
    if(length<1)return(nullptr);

    VkImageCreateInfo imageCreateInfo;

    imageCreateInfo.sType                   = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext                   = nullptr;
    imageCreateInfo.flags                   = 0;
    imageCreateInfo.imageType               = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format                  = format;
    imageCreateInfo.extent.width            = length;
    imageCreateInfo.extent.height           = 1;
    imageCreateInfo.extent.depth            = 1;
    imageCreateInfo.mipLevels               = 1;
    imageCreateInfo.arrayLayers             = 1;
    imageCreateInfo.samples                 = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.usage                   = usage;
    imageCreateInfo.sharingMode             = VkSharingMode(SharingMode::Exclusive);
    imageCreateInfo.queueFamilyIndexCount   = 0;
    imageCreateInfo.pQueueFamilyIndices     = nullptr;
    imageCreateInfo.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.tiling                  = VkImageTiling(tiling);

    VkImage image;

    if(vkCreateImage(attr->device,&imageCreateInfo, nullptr, &image)!=VK_SUCCESS)
        return(nullptr);

    return image;
}

VkImage Device::CreateImage1DArray(const VkFormat format,const uint32_t length,const uint32_t layer,const uint usage,const ImageTiling tiling)
{
    if(!CheckVulkanFormat(format))return(nullptr);
    if(length<1||layer<1)return(nullptr);

    VkImageCreateInfo imageCreateInfo;

    imageCreateInfo.sType                   = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext                   = nullptr;
    imageCreateInfo.flags                   = 0;
    imageCreateInfo.imageType               = VK_IMAGE_TYPE_1D;
    imageCreateInfo.format                  = format;
    imageCreateInfo.extent.width            = length;
    imageCreateInfo.extent.height           = 1;
    imageCreateInfo.extent.depth            = 1;
    imageCreateInfo.mipLevels               = 1;
    imageCreateInfo.arrayLayers             = layer;
    imageCreateInfo.samples                 = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.usage                   = usage;
    imageCreateInfo.sharingMode             = VkSharingMode(SharingMode::Exclusive);
    imageCreateInfo.queueFamilyIndexCount   = 0;
    imageCreateInfo.pQueueFamilyIndices     = nullptr;
    imageCreateInfo.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.tiling                  = VkImageTiling(tiling);

    VkImage image;

    if(vkCreateImage(attr->device,&imageCreateInfo, nullptr, &image)!=VK_SUCCESS)
        return(nullptr);

    return image;
}

VkImage Device::CreateImage2D(const VkFormat format,const uint32_t width,const uint32_t height,const uint usage,const ImageTiling tiling)
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
    imageCreateInfo.sharingMode             = VkSharingMode(SharingMode::Exclusive);
    imageCreateInfo.queueFamilyIndexCount   = 0;
    imageCreateInfo.pQueueFamilyIndices     = nullptr;
    imageCreateInfo.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.tiling                  = VkImageTiling(tiling);

    VkImage image;

    if(vkCreateImage(attr->device,&imageCreateInfo, nullptr, &image)!=VK_SUCCESS)
        return(nullptr);

    return image;
}

VkImage Device::CreateImage2DArray(const VkFormat format,const uint32_t width,const uint32_t height,const uint32_t layer,const uint usage,const ImageTiling tiling)
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
    imageCreateInfo.arrayLayers             = layer;
    imageCreateInfo.samples                 = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.usage                   = usage;
    imageCreateInfo.sharingMode             = VkSharingMode(SharingMode::Exclusive);
    imageCreateInfo.queueFamilyIndexCount   = 0;
    imageCreateInfo.pQueueFamilyIndices     = nullptr;
    imageCreateInfo.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.tiling                  = VkImageTiling(tiling);

    VkImage image;

    if(vkCreateImage(attr->device,&imageCreateInfo, nullptr, &image)!=VK_SUCCESS)
        return(nullptr);

    return image;
}

VkImage Device::CreateImage3D(const VkFormat format,const uint32_t width,const uint32_t height,const uint32_t depth,const uint usage,const ImageTiling tiling)
{
    if(!CheckVulkanFormat(format))return(nullptr);
    if(width<1||height<1)return(nullptr);

    VkImageCreateInfo imageCreateInfo;

    imageCreateInfo.sType                   = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext                   = nullptr;
    imageCreateInfo.flags                   = 0;
    imageCreateInfo.imageType               = VK_IMAGE_TYPE_3D;
    imageCreateInfo.format                  = format;
    imageCreateInfo.extent.width            = width;
    imageCreateInfo.extent.height           = height;
    imageCreateInfo.extent.depth            = depth;
    imageCreateInfo.mipLevels               = 1;
    imageCreateInfo.arrayLayers             = 1;
    imageCreateInfo.samples                 = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.usage                   = usage;
    imageCreateInfo.sharingMode             = VkSharingMode(SharingMode::Exclusive);
    imageCreateInfo.queueFamilyIndexCount   = 0;
    imageCreateInfo.pQueueFamilyIndices     = nullptr;
    imageCreateInfo.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.tiling                  = VkImageTiling(tiling);

    VkImage image;

    if(vkCreateImage(attr->device,&imageCreateInfo, nullptr, &image)!=VK_SUCCESS)
        return(nullptr);

    return image;
}

VkImage Device::CreateCubemap(const VkFormat format,const uint32_t width,const uint32_t height,const uint usage,const ImageTiling tiling)
{
    if(!CheckVulkanFormat(format))return(nullptr);
    if(width<1||height<1)return(nullptr);

    VkImageCreateInfo imageCreateInfo;

    imageCreateInfo.sType                   = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext                   = nullptr;
    imageCreateInfo.flags                   = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    imageCreateInfo.imageType               = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format                  = format;
    imageCreateInfo.extent.width            = width;
    imageCreateInfo.extent.height           = height;
    imageCreateInfo.extent.depth            = 1;
    imageCreateInfo.mipLevels               = 1;
    imageCreateInfo.arrayLayers             = 6;
    imageCreateInfo.samples                 = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.usage                   = usage;
    imageCreateInfo.sharingMode             = VkSharingMode(SharingMode::Exclusive);
    imageCreateInfo.queueFamilyIndexCount   = 0;
    imageCreateInfo.pQueueFamilyIndices     = nullptr;
    imageCreateInfo.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.tiling                  = VkImageTiling(tiling);

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
