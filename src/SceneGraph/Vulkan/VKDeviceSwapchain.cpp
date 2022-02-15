#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKDeviceAttribute.h>
#include<hgl/graph/VKPhysicalDevice.h>

VK_NAMESPACE_BEGIN
namespace
{
    //VkExtent2D SwapchainExtentClamp(const VkSurfaceCapabilitiesKHR &surface_caps,const VkExtent2D &acquire_extent)
    //{
    //    VkExtent2D swapchain_extent;

    //    swapchain_extent.width  =hgl_clamp(acquire_extent.width,    surface_caps.minImageExtent.width,  surface_caps.maxImageExtent.width   );
    //    swapchain_extent.height =hgl_clamp(acquire_extent.height,   surface_caps.minImageExtent.height, surface_caps.maxImageExtent.height  );

    //    return swapchain_extent;
    //}

    VkSwapchainKHR CreateSwapChain(const GPUDeviceAttribute *dev_attr,const VkExtent2D &extent)
    {
        VkSwapchainCreateInfoKHR swapchain_ci;

        swapchain_ci.sType                  =VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_ci.pNext                  =nullptr;
        swapchain_ci.flags                  =0;
        swapchain_ci.surface                =dev_attr->surface;
        swapchain_ci.minImageCount          =3;//rsa->surface_caps.minImageCount;
        swapchain_ci.imageFormat            =dev_attr->surface_format.format;
        swapchain_ci.imageColorSpace        =dev_attr->surface_format.colorSpace;
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

bool GPUDevice::CreateSwapchainFBO(Swapchain *swapchain)
{
    if(vkGetSwapchainImagesKHR(attr->device,swapchain->swap_chain,&(swapchain->color_count),nullptr)!=VK_SUCCESS)
        return(false);

    AutoDeleteArray<VkImage> sc_images(swapchain->color_count);

    if(vkGetSwapchainImagesKHR(attr->device,swapchain->swap_chain,&(swapchain->color_count),sc_images)!=VK_SUCCESS)
        return(false);

    swapchain->sc_depth     =CreateTexture2D(new SwapchainDepthTextureCreateInfo(attr->physical_device->GetDepthFormat(),swapchain->extent));

    if(!swapchain->sc_depth)
        return(false);

    swapchain->sc_color     =hgl_zero_new<Texture2D *>(swapchain->color_count);
    swapchain->render_frame =hgl_zero_new<Framebuffer *>(swapchain->color_count);

    for(uint32_t i=0;i<swapchain->color_count;i++)
    {
        swapchain->sc_color[i]=CreateTexture2D(new SwapchainColorTextureCreateInfo(attr->surface_format.format,swapchain->extent,sc_images[i]));

        if(!swapchain->sc_color[i])
            return(false);

        swapchain->render_frame[i]=CreateFramebuffer(   device_render_pass,
                                                        swapchain->sc_color[i]->GetImageView(),
                                                        swapchain->sc_depth->GetImageView());
    }

    return(true);
}

Swapchain *GPUDevice::CreateSwapchain(const VkExtent2D &acquire_extent)
{
    Swapchain *swapchain=new Swapchain;

    swapchain->device          =attr->device;
    swapchain->extent          =acquire_extent;

    swapchain->swap_chain      =CreateSwapChain(attr,acquire_extent);

    if(swapchain->swap_chain)
    if(CreateSwapchainFBO(swapchain))
        return(swapchain);

    delete swapchain;
    swapchain=nullptr;

    return(nullptr);
}
VK_NAMESPACE_END
