#include<hgl/graph/vulkan/VKSwapchain.h>
#include<hgl/graph/vulkan/VKDeviceAttribute.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>

VK_NAMESPACE_BEGIN
Swapchain::~Swapchain()
{
    SAFE_CLEAR(sc_depth);
    sc_color.Clear();

    if(swap_chain)
    {
        vkDestroySwapchainKHR(device,swap_chain,VK_NULL_HANDLE);
        swap_chain=VK_NULL_HANDLE;
    }

    swap_chain_count=0;
}

namespace
{
    VkExtent2D SwapchainExtentClamp(const VkSurfaceCapabilitiesKHR &surface_caps,const VkExtent2D &acquire_extent)
    {
        VkExtent2D swapchain_extent;

        swapchain_extent.width  =hgl_clamp(acquire_extent.width,    surface_caps.minImageExtent.width,  surface_caps.maxImageExtent.width   );
        swapchain_extent.height =hgl_clamp(acquire_extent.height,   surface_caps.minImageExtent.height, surface_caps.maxImageExtent.height  );

        return swapchain_extent;
    }
    
    VkSwapchainKHR CreateSwapChain(const DeviceAttribute *dev_attr,const VkExtent2D &extent)
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
            swapchain_ci.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;
        }

        VkSwapchainKHR swap_chain;

        if(vkCreateSwapchainKHR(dev_attr->device,&swapchain_ci,nullptr,&swap_chain)==VK_SUCCESS)
            return(swap_chain);

        return(VK_NULL_HANDLE);
    }

    bool CreateSwapchainColorTexture(Swapchain *sa,const DeviceAttribute *dev_attr)
    {
        if(vkGetSwapchainImagesKHR(dev_attr->device,sa->swap_chain,&(sa->swap_chain_count),nullptr)!=VK_SUCCESS)
            return(false);

        AutoDeleteArray<VkImage> sc_images=new VkImage[sa->swap_chain_count];

        if(vkGetSwapchainImagesKHR(dev_attr->device,sa->swap_chain,&(sa->swap_chain_count),sc_images)!=VK_SUCCESS)
        {
            delete sc_images;
            return(false);
        }

        VkImage *ip=sc_images;
        Texture2D *tex;

        for(uint32_t i=0; i<sa->swap_chain_count; i++)
        {
            tex=VK_NAMESPACE::CreateTexture2D(  dev_attr->device,
                                                dev_attr->format,
                                                sa->extent.width,
                                                sa->extent.height,
                                                VK_IMAGE_ASPECT_COLOR_BIT,
                                                *ip,
                                                VK_IMAGE_LAYOUT_UNDEFINED);

            sa->sc_color.Add(tex);

            ++ip;
        }

        return(true);
    }

    bool CreateSwapchainDepthTexture(Swapchain *sa,const DeviceAttribute *dev_attr)
    {
        const VkFormat depth_format=dev_attr->physical_device->GetDepthFormat();

        const VkFormatProperties props=dev_attr->physical_device->GetFormatProperties(depth_format);

        sa->sc_depth=VK_NAMESPACE::CreateTexture2D( dev_attr->device,dev_attr->physical_device,
                                                    depth_format,
                                                    sa->extent.width,
                                                    sa->extent.height,
                                                    VK_IMAGE_ASPECT_DEPTH_BIT,
                                                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                                    (props.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)?VK_IMAGE_TILING_OPTIMAL:VK_IMAGE_TILING_LINEAR);

        return sa->sc_depth;
    }
}//namespace

Swapchain *CreateSwapchain(const DeviceAttribute *attr,const VkExtent2D &acquire_extent)
{
    AutoDelete<Swapchain> sc=new Swapchain;

    sc->device          =attr->device;
    sc->extent          =SwapchainExtentClamp(attr->surface_caps,acquire_extent);
    sc->graphics_queue  =attr->graphics_queue;
    sc->swap_chain      =CreateSwapChain(attr,sc->extent);

    if(!sc->swap_chain)
        return(nullptr);
            
    if(!CreateSwapchainColorTexture(sc,attr))
        return(nullptr);
    
    if(!CreateSwapchainDepthTexture(sc,attr))
        return(nullptr);
        
    return sc.Finish();
}
VK_NAMESPACE_END
