#include"RenderSurface.h"
#include"VKInstance.h"
#include<hgl/type/Smart.h>

VK_NAMESPACE_BEGIN

VkSurfaceKHR CreateSurface(VkInstance,Window *);

namespace
{
    template<typename T> T Clamp(const T &cur,const T &min_value,const T &max_value)
    {
        if(cur<min_value)return min_value;
        if(cur>max_value)return max_value;

        return cur;
    }

    VkExtent2D GetSwapchainExtent(VkSurfaceCapabilitiesKHR &surface_caps,uint32_t width,uint32_t height)
    {
        if(surface_caps.currentExtent.width==0xFFFFFFFF)
        {
            VkExtent2D swapchain_extent;

            swapchain_extent.width=Clamp(width,surface_caps.minImageExtent.width,surface_caps.maxImageExtent.width);
            swapchain_extent.height=Clamp(height,surface_caps.minImageExtent.height,surface_caps.maxImageExtent.height);

            return swapchain_extent;
        }
        else
        {
            return surface_caps.currentExtent;
        }
    }

    VkDevice CreateDevice(VkInstance instance,VkPhysicalDevice physical_device,uint32_t graphics_family)
    {
        float queue_priorities[1]={0.0};

        VkDeviceQueueCreateInfo queue_info;
        queue_info.queueFamilyIndex=graphics_family;
        queue_info.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.pNext=nullptr;
        queue_info.queueCount=1;
        queue_info.pQueuePriorities=queue_priorities;

        VkDeviceCreateInfo create_info={};
        const char *ext_list[1]={VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        create_info.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.pNext=nullptr;
        create_info.queueCreateInfoCount=1;
        create_info.pQueueCreateInfos=&queue_info;
        create_info.enabledExtensionCount=1;
        create_info.ppEnabledExtensionNames=ext_list;
        create_info.enabledLayerCount=0;
        create_info.ppEnabledLayerNames=nullptr;
        create_info.pEnabledFeatures=nullptr;

        VkDevice device;

        if(vkCreateDevice(physical_device,&create_info,nullptr,&device)==VK_SUCCESS)
            return device;

        return nullptr;
    }

    VkCommandPool CreateCommandPool(VkDevice device,uint32_t graphics_family)
    {
        VkCommandPoolCreateInfo cmd_pool_info={};

        cmd_pool_info.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmd_pool_info.pNext=nullptr;
        cmd_pool_info.queueFamilyIndex=graphics_family;
        cmd_pool_info.flags=0;

        VkCommandPool cmd_pool;

        if(vkCreateCommandPool(device,&cmd_pool_info,nullptr,&cmd_pool)==VK_SUCCESS)
            return cmd_pool;

        return(nullptr);
    }

    VkSwapchainKHR CreateSwapChain(RenderSurfaceAttribute *rsa)
    {
        VkSwapchainCreateInfoKHR swapchain_ci={};

        swapchain_ci.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_ci.pNext=nullptr;
        swapchain_ci.surface=rsa->surface;
        swapchain_ci.minImageCount=rsa->surface_caps.minImageCount;
        swapchain_ci.imageFormat=rsa->format;
        swapchain_ci.imageExtent=rsa->swapchain_extent;
        swapchain_ci.preTransform=rsa->preTransform;
        swapchain_ci.compositeAlpha=rsa->compositeAlpha;
        swapchain_ci.imageArrayLayers=1;
        swapchain_ci.presentMode=VK_PRESENT_MODE_FIFO_KHR;
        swapchain_ci.oldSwapchain=VK_NULL_HANDLE;
        swapchain_ci.clipped=true;
        swapchain_ci.imageColorSpace=VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        swapchain_ci.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain_ci.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;
        swapchain_ci.queueFamilyIndexCount=0;
        swapchain_ci.pQueueFamilyIndices=nullptr;

        uint32_t queueFamilyIndices[2]={(uint32_t)rsa->graphics_family, (uint32_t)rsa->present_family};
        if(rsa->graphics_family!=rsa->present_family)
        {
            // If the graphics and present queues are from different queue families,
            // we either have to explicitly transfer ownership of images between
            // the queues, or we have to create the swapchain with imageSharingMode
            // as VK_SHARING_MODE_CONCURRENT
            swapchain_ci.imageSharingMode=VK_SHARING_MODE_CONCURRENT;
            swapchain_ci.queueFamilyIndexCount=2;
            swapchain_ci.pQueueFamilyIndices=queueFamilyIndices;
        }

        VkSwapchainKHR swap_chain;

        if(vkCreateSwapchainKHR(rsa->device,&swapchain_ci,nullptr,&swap_chain)==VK_SUCCESS)
            return(swap_chain);

        return(nullptr);
    }

    bool CreateImageView(RenderSurfaceAttribute *rsa)
    {
        uint32_t count;

        if(vkGetSwapchainImagesKHR(rsa->device,rsa->swap_chain,&count,nullptr)!=VK_SUCCESS)
            return(false);

        rsa->sc_images.SetCount(count);

        if(vkGetSwapchainImagesKHR(rsa->device,rsa->swap_chain,&count,rsa->sc_images.GetData())!=VK_SUCCESS)
        {
            rsa->sc_images.Clear();
            return(false);
        }

        rsa->sc_image_views.SetCount(count);

        VkImage *ip=rsa->sc_images.GetData();
        VkImageView *vp=rsa->sc_image_views.GetData();
        for(uint32_t i=0; i<count; i++)
        {
            VkImageViewCreateInfo color_image_view={};

            color_image_view.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            color_image_view.pNext=nullptr;
            color_image_view.flags=0;
            color_image_view.image=*ip;
            color_image_view.viewType=VK_IMAGE_VIEW_TYPE_2D;
            color_image_view.format=rsa->format;
            color_image_view.components.r=VK_COMPONENT_SWIZZLE_R;
            color_image_view.components.g=VK_COMPONENT_SWIZZLE_G;
            color_image_view.components.b=VK_COMPONENT_SWIZZLE_B;
            color_image_view.components.a=VK_COMPONENT_SWIZZLE_A;
            color_image_view.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
            color_image_view.subresourceRange.baseMipLevel=0;
            color_image_view.subresourceRange.levelCount=1;
            color_image_view.subresourceRange.baseArrayLayer=0;
            color_image_view.subresourceRange.layerCount=1;

            if(vkCreateImageView(rsa->device,&color_image_view,nullptr,vp)!=VK_SUCCESS)
                return(false);

            ++ip;
            ++vp;
        }

        return(true);
    }
}//namespace

RenderSurface *CreateRenderSuface(VkInstance inst,VkPhysicalDevice physical_device,Window *win)
{
    VkSurfaceKHR surface=CreateSurface(inst,win);

    if(!surface)
        return(nullptr);

    RefRenderSurfaceAttribute rsa=new RenderSurfaceAttribute(inst,physical_device,surface);

    rsa->swapchain_extent=GetSwapchainExtent(rsa->surface_caps,win->GetWidth(),win->GetHeight());

    if(rsa->graphics_family==ERROR_FAMILY_INDEX)
        return(nullptr);

    rsa->device=CreateDevice(inst,physical_device,rsa->graphics_family);

    if(!rsa->device)
        return(nullptr);

    rsa->cmd_pool=CreateCommandPool(rsa->device,rsa->graphics_family);

    if(!rsa->cmd_pool)
        return(nullptr);

    rsa->swap_chain=CreateSwapChain(rsa);

    if(!rsa->swap_chain)
        return(nullptr);

    if(!CreateImageView(rsa))
        return(nullptr);

    return(new RenderSurface(rsa));
}
VK_NAMESPACE_END
