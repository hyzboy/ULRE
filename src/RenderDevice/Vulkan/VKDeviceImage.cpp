#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKImageCreateInfo.h>

VK_NAMESPACE_BEGIN
VkImage Device::CreateImage1D(const VkFormat format,const uint32_t length,const uint usage,const ImageTiling tiling)
{
    if(!CheckVulkanFormat(format))return(nullptr);
    if(length<1)return(nullptr);

    Image1DCreateInfo imageCreateInfo(usage,tiling,format,length);

    VkImage image;

    if(vkCreateImage(attr->device,&imageCreateInfo, nullptr, &image)!=VK_SUCCESS)
        return(nullptr);

    return image;
}

VkImage Device::CreateImage1DArray(const VkFormat format,const uint32_t length,const uint32_t layer,const uint usage,const ImageTiling tiling)
{
    if(!CheckVulkanFormat(format))return(nullptr);
    if(length<1||layer<1)return(nullptr);

    Image1DArrayCreateInfo imageCreateInfo(usage,tiling,format,length,layer);

    VkImage image;

    if(vkCreateImage(attr->device,&imageCreateInfo, nullptr, &image)!=VK_SUCCESS)
        return(nullptr);

    return image;
}

VkImage Device::CreateImage2D(const VkFormat format,const uint32_t width,const uint32_t height,const uint usage,const ImageTiling tiling)
{
    if(!CheckVulkanFormat(format))return(nullptr);
    if(width<1||height<1)return(nullptr);

    Image2DCreateInfo imageCreateInfo(usage,tiling,format,width,height);

    VkImage image;

    if(vkCreateImage(attr->device,&imageCreateInfo, nullptr, &image)!=VK_SUCCESS)
        return(nullptr);

    return image;
}

VkImage Device::CreateImage2DArray(const VkFormat format,const uint32_t width,const uint32_t height,const uint32_t layer,const uint usage,const ImageTiling tiling)
{
    if(!CheckVulkanFormat(format))return(nullptr);
    if(width<1||height<1)return(nullptr);

    Image2DArrayCreateInfo imageCreateInfo(usage,tiling,format,width,height,layer);

    VkImage image;

    if(vkCreateImage(attr->device,&imageCreateInfo, nullptr, &image)!=VK_SUCCESS)
        return(nullptr);

    return image;
}

VkImage Device::CreateImage3D(const VkFormat format,const VkExtent3D &extent,const uint usage,const ImageTiling tiling)
{
    if(!CheckVulkanFormat(format))return(nullptr);
    if(extent.width<1||extent.height<1||extent.depth<1)return(nullptr);

    Image3DCreateInfo imageCreateInfo(usage,tiling,format,extent);

    VkImage image;

    if(vkCreateImage(attr->device,&imageCreateInfo, nullptr, &image)!=VK_SUCCESS)
        return(nullptr);

    return image;
}

VkImage Device::CreateCubemap(const VkFormat format,const uint32_t width,const uint32_t height,const uint usage,const ImageTiling tiling)
{
    if(!CheckVulkanFormat(format))return(nullptr);
    if(width<1||height<1)return(nullptr);

    ImageCubemapCreateInfo imageCreateInfo(usage,tiling,format,width,height);

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
