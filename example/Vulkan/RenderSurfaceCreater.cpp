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

    bool memory_type_from_properties(VkPhysicalDeviceMemoryProperties memory_properties,uint32_t typeBits,VkFlags requirements_mask,uint32_t *typeIndex)
    {
        // Search memtypes to find first index with those properties
        for(uint32_t i=0; i<memory_properties.memoryTypeCount; i++)
        {
            if((typeBits&1)==1)
            {
                // Type is available, does it match user properties?
                if((memory_properties.memoryTypes[i].propertyFlags&requirements_mask)==requirements_mask)
                {
                    *typeIndex=i;
                    return true;
                }
            }
            typeBits>>=1;
        }
        // No memory types matched, return failure
        return false;
    }

    bool CreateDepthBuffer(RenderSurfaceAttribute *rsa)
    {
        VkImageCreateInfo image_info={};

        const VkFormat depth_format=VK_FORMAT_D16_UNORM;
        VkFormatProperties props;

        vkGetPhysicalDeviceFormatProperties(rsa->physical_device,depth_format,&props);

        if(props.linearTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            image_info.tiling=VK_IMAGE_TILING_LINEAR;
        else
        if(props.optimalTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            image_info.tiling=VK_IMAGE_TILING_OPTIMAL;
        else
            return(false);

        image_info.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.pNext=nullptr;
        image_info.imageType=VK_IMAGE_TYPE_2D;
        image_info.format=depth_format;
        image_info.extent.width=rsa->swapchain_extent.width;
        image_info.extent.height=rsa->swapchain_extent.height;
        image_info.extent.depth=1;
        image_info.mipLevels=1;
        image_info.arrayLayers=1;
        image_info.samples=VK_SAMPLE_COUNT_1_BIT;
        image_info.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        image_info.queueFamilyIndexCount=0;
        image_info.pQueueFamilyIndices=nullptr;
        image_info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        image_info.flags=0;

        VkImageViewCreateInfo view_info={};
        view_info.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.pNext=nullptr;
        view_info.image=VK_NULL_HANDLE;
        view_info.format=depth_format;
        view_info.components.r=VK_COMPONENT_SWIZZLE_R;
        view_info.components.g=VK_COMPONENT_SWIZZLE_G;
        view_info.components.b=VK_COMPONENT_SWIZZLE_B;
        view_info.components.a=VK_COMPONENT_SWIZZLE_A;
        view_info.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
        view_info.subresourceRange.baseMipLevel=0;
        view_info.subresourceRange.levelCount=1;
        view_info.subresourceRange.baseArrayLayer=0;
        view_info.subresourceRange.layerCount=1;
        view_info.viewType=VK_IMAGE_VIEW_TYPE_2D;
        view_info.flags=0;

        rsa->depth.format=depth_format;

        if(vkCreateImage(rsa->device,&image_info,nullptr,&rsa->depth.image)!=VK_SUCCESS)
            return(false);

        VkMemoryRequirements mem_reqs;
        vkGetImageMemoryRequirements(rsa->device,rsa->depth.image,&mem_reqs);

        VkMemoryAllocateInfo mem_alloc={};
        mem_alloc.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mem_alloc.pNext=nullptr;
        mem_alloc.allocationSize=0;
        mem_alloc.memoryTypeIndex=0;
        mem_alloc.allocationSize=mem_reqs.size;

        bool pass=memory_type_from_properties(rsa->memory_properties,mem_reqs.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&mem_alloc.memoryTypeIndex);

        if(vkAllocateMemory(rsa->device,&mem_alloc,nullptr,&rsa->depth.mem)!=VK_SUCCESS)
            return(false);

        if(vkBindImageMemory(rsa->device,rsa->depth.image,rsa->depth.mem,0)!=VK_SUCCESS)
            return(false);

        view_info.image=rsa->depth.image;
        if(vkCreateImageView(rsa->device,&view_info,nullptr,&rsa->depth.view)!=VK_SUCCESS)
            return(false);

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

    if(!CreateDepthBuffer(rsa))
        return(nullptr);

    return(new RenderSurface(rsa));
}
VK_NAMESPACE_END
