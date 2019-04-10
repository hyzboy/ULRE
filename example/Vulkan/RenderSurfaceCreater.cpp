#include"RenderSurface.h"
#include"VKInstance.h"
#include<hgl/type/Smart.h>

VK_NAMESPACE_BEGIN

VkSurfaceKHR CreateSurface(VkInstance,Window *);

namespace
{
    VkExtent2D GetSwapchainExtent(VkSurfaceCapabilitiesKHR &surface_caps,int width,int height)
    {
        if(surface_caps.currentExtent.width==0xFFFFFFFF)
        {
            VkExtent2D swapchain_extent;

            swapchain_extent.width=width;
            swapchain_extent.height=height;

            if(swapchain_extent.width<surface_caps.minImageExtent.width)swapchain_extent.width=surface_caps.minImageExtent.width;else
                if(swapchain_extent.width>surface_caps.maxImageExtent.width)swapchain_extent.width=surface_caps.maxImageExtent.width;

                if(swapchain_extent.height<surface_caps.minImageExtent.height)swapchain_extent.height=surface_caps.minImageExtent.height;else
                    if(swapchain_extent.height>surface_caps.maxImageExtent.height)swapchain_extent.height=surface_caps.maxImageExtent.height;

            return swapchain_extent;
        }
        else
        {
            return surface_caps.currentExtent;
        }
    }

    VkDevice CreateDevice(VkInstance instance,VkPhysicalDevice physical_device,int family_index)
    {
        float queue_priorities[1]={0.0};

        VkDeviceQueueCreateInfo queue_info;
        queue_info.queueFamilyIndex=family_index;
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

    VkCommandPool CreateCommandPool(VkDevice device,int family_index)
    {
        VkCommandPoolCreateInfo cmd_pool_info={};

        cmd_pool_info.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmd_pool_info.pNext=nullptr;
        cmd_pool_info.queueFamilyIndex=family_index;
        cmd_pool_info.flags=0;

        VkCommandPool cmd_pool;

        if(vkCreateCommandPool(device,&cmd_pool_info,nullptr,&cmd_pool)==VK_SUCCESS)
            return cmd_pool;

        return(nullptr);
    }
}//namespace

RenderSurface *CreateRenderSuface(VkInstance inst,VkPhysicalDevice physical_device,Window *win)
{
    VkSurfaceKHR surface=CreateSurface(inst,win);

    if(!surface)
        return(nullptr);

    RefRenderSurfaceAttribute rsa=new RenderSurfaceAttribute(inst,physical_device,surface);

    rsa->swapchain_extent=GetSwapchainExtent(rsa->surface_caps,win->GetWidth(),win->GetHeight());

    if(rsa->family_index==-1)
        return(nullptr);

    rsa->device=CreateDevice(inst,physical_device,rsa->family_index);

    if(!rsa->device)
        return(nullptr);

    rsa->cmd_pool=CreateCommandPool(rsa->device,rsa->family_index);

    if(!rsa->cmd_pool)
        return(nullptr);

    return(new RenderSurface(rsa));
}
VK_NAMESPACE_END
