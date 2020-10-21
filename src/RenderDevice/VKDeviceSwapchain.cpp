#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKDeviceAttribute.h>
#include<hgl/graph/VKPhysicalDevice.h>

VK_NAMESPACE_BEGIN
namespace
{
    VkExtent2D SwapchainExtentClamp(const VkSurfaceCapabilitiesKHR &surface_caps,const VkExtent2D &acquire_extent)
    {
        VkExtent2D swapchain_extent;

        swapchain_extent.width  =hgl_clamp(acquire_extent.width,    surface_caps.minImageExtent.width,  surface_caps.maxImageExtent.width   );
        swapchain_extent.height =hgl_clamp(acquire_extent.height,   surface_caps.minImageExtent.height, surface_caps.maxImageExtent.height  );

        return swapchain_extent;
    }

    VkSwapchainKHR CreateSwapChain(const RenderDeviceAttribute *dev_attr,const VkExtent2D &extent)
    {
        VkSwapchainCreateInfoKHR swapchain_ci;

        swapchain_ci.sType                  =VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_ci.pNext                  =nullptr;
        swapchain_ci.flags                  =0;
        swapchain_ci.surface                =dev_attr->surface;
        swapchain_ci.minImageCount          =3;//rsa->surface_caps.minImageCount;
        swapchain_ci.imageFormat            =dev_attr->format;
        swapchain_ci.imageColorSpace        =VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swapchain_ci.imageExtent            =extent;
        swapchain_ci.imageArrayLayers       =1;
        swapchain_ci.imageUsage             =VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain_ci.queueFamilyIndexCount  =0;
        swapchain_ci.pQueueFamilyIndices    =nullptr;
        swapchain_ci.preTransform           =dev_attr->preTransform;
        swapchain_ci.compositeAlpha         =dev_attr->compositeAlpha;
        swapchain_ci.presentMode            =VK_PRESENT_MODE_FIFO_KHR;
        swapchain_ci.clipped                =VK_TRUE;
        swapchain_ci.oldSwapchain           =VK_NULL_HANDLE;

        if(dev_attr->surface_caps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            swapchain_ci.imageUsage|=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        if(dev_attr->surface_caps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            swapchain_ci.imageUsage|=VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        uint32_t queueFamilyIndices[2]={dev_attr->graphics_family, dev_attr->present_family};
        if(dev_attr->graphics_family!=dev_attr->present_family)
        {
            // If the graphics and present queues are from different queue families,
            // we either have to explicitly transfer ownership of images between
            // the queues, or we have to create the swapchain with imageSharingMode
            // as VK_SHARING_MODE_CONCURRENT
            swapchain_ci.imageSharingMode=VK_SHARING_MODE_CONCURRENT;
            swapchain_ci.queueFamilyIndexCount=2;
            swapchain_ci.pQueueFamilyIndices=queueFamilyIndices;
        }
        else
        {
            swapchain_ci.imageSharingMode = VkSharingMode(SharingMode::Exclusive);
        }

        VkSwapchainKHR swap_chain;

        if(vkCreateSwapchainKHR(dev_attr->device,&swapchain_ci,nullptr,&swap_chain)==VK_SUCCESS)
            return(swap_chain);

        return(VK_NULL_HANDLE);
    }
}//namespace

bool RenderDevice::CreateSwapchainColorTexture()
{
    if(vkGetSwapchainImagesKHR(attr->device,swapchain->swap_chain,&(swapchain->swap_chain_count),nullptr)!=VK_SUCCESS)
        return(false);

    AutoDeleteArray<VkImage> sc_images(swapchain->swap_chain_count);

    if(vkGetSwapchainImagesKHR(attr->device,swapchain->swap_chain,&(swapchain->swap_chain_count),sc_images)!=VK_SUCCESS)
        return(false);

    VkImage *ip=sc_images;
    Texture2D *tex;

    for(uint32_t i=0; i<swapchain->swap_chain_count; i++)
    {
        tex=CreateTexture2D(attr->format,
            swapchain->extent.width,
            swapchain->extent.height,
            VK_IMAGE_ASPECT_COLOR_BIT,
            *ip,
            VK_IMAGE_LAYOUT_UNDEFINED);

        swapchain->sc_color.Add(tex);

        ++ip;
    }

    return(true);
}

bool RenderDevice::CreateSwapchainDepthTexture()
{
    const VkFormat depth_format=attr->physical_device->GetDepthFormat();

    const VkFormatProperties props=attr->physical_device->GetFormatProperties(depth_format);

    swapchain->sc_depth=CreateTexture2D(depth_format,
        swapchain->extent.width,
        swapchain->extent.height,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        (props.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)?ImageTiling::Optimal:ImageTiling::Linear);

    return swapchain->sc_depth;
}

bool RenderDevice::CreateSwapchain(const VkExtent2D &acquire_extent)
{
    swapchain=new Swapchain;

    swapchain->device          =attr->device;
    swapchain->extent          =SwapchainExtentClamp(attr->surface_caps,acquire_extent);
    swapchain->graphics_queue  =attr->graphics_queue;
    swapchain->swap_chain      =CreateSwapChain(attr,swapchain->extent);

    if(swapchain->swap_chain)
    if(CreateSwapchainColorTexture())
    if(CreateSwapchainDepthTexture())
        return(true);

    delete swapchain;
    swapchain=nullptr;

    return(false);
}
VK_NAMESPACE_END
