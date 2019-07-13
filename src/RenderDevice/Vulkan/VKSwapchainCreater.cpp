#include<hgl/graph/vulkan/VKSwapchain.h>

VK_NAMESPACE_BEGIN
namespace
{
    template<typename T> T Clamp(const T &cur,const T &min_value,const T &max_value)
    {
        if(cur<min_value)return min_value;
        if(cur>max_value)return max_value;

        return cur;
    }

    VkExtent2D GetSwapchainExtent(VkSurfaceCapabilitiesKHR &surface_caps,const VkExtent2D &acquire_extent)
    {
        if(surface_caps.currentExtent.width==UINT32_MAX)
        {
            VkExtent2D swapchain_extent;

            swapchain_extent.width  =Clamp(acquire_extent.width,    surface_caps.minImageExtent.width,  surface_caps.maxImageExtent.width   );
            swapchain_extent.height =Clamp(acquire_extent.height,   surface_caps.minImageExtent.height, surface_caps.maxImageExtent.height  );

            return swapchain_extent;
        }
        else
        {
            return surface_caps.currentExtent;
        }
    }

    bool CreateSwapchainColorTexture(DeviceAttribute *rsa)
    {
        uint32_t count;

        if(vkGetSwapchainImagesKHR(rsa->device,rsa->swap_chain,&count,nullptr)!=VK_SUCCESS)
            return(false);

        VkImage *sc_images=new VkImage[count];

        if(vkGetSwapchainImagesKHR(rsa->device,rsa->swap_chain,&count,sc_images)!=VK_SUCCESS)
        {
            delete sc_images;
            return(false);
        }

        VkImage *ip=sc_images;
        Texture2D *tex;

        for(uint32_t i=0; i<count; i++)
        {
            tex=VK_NAMESPACE::CreateTexture2D(  rsa->device,
                                                rsa->format,
                                                rsa->swapchain_extent.width,
                                                rsa->swapchain_extent.height,
                                                VK_IMAGE_ASPECT_COLOR_BIT,
                                                *ip,
                                                VK_IMAGE_LAYOUT_UNDEFINED);

            rsa->sc_texture.Add(tex);

            ++ip;
        }

        delete[] sc_images;
        return(true);
    }

    bool CreateSwapchainDepthTexture(DeviceAttribute *rsa)
    {
        const VkFormat depth_format=rsa->physical_device->GetDepthFormat();

        const VkFormatProperties props=rsa->physical_device->GetFormatProperties(depth_format);

        rsa->sc_depth=VK_NAMESPACE::CreateTexture2D(rsa->device,rsa->physical_device,
                                                    depth_format,
                                                    rsa->swapchain_extent.width,
                                                    rsa->swapchain_extent.height,
                                                    VK_IMAGE_ASPECT_DEPTH_BIT,
                                                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                    VK_IMAGE_LAYOUT_UNDEFINED,
                                                    (props.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)?VK_IMAGE_TILING_OPTIMAL:VK_IMAGE_TILING_LINEAR);

        return(true);
    }

    bool CreateSwapchinAndDepthBuffer(DeviceAttribute *attr)
    {
        attr->swapchain_extent=GetSwapchainExtent(attr->surface_caps,width,height);

        attr->swap_chain=CreateSwapChain(attr);

        if(!attr->swap_chain)
            return(false);

        if(!CreateSwapchainColorTexture(attr))
            return(false);

        if(!CreateSwapchainDepthTexture(attr))
            return(false);

        return(true);
    }
}//namespace
VK_NAMESPACE_END
