#include"RenderSurface.h"

VK_NAMESPACE_BEGIN
RenderSurface::RenderSurface(Window *w,VkInstance inst,VkPhysicalDevice pd)
{
    win=w;
    instance=inst;
    physical_device=pd;
    family_index=-1;
    device=nullptr;
    cmd_pool=nullptr;

    vkGetPhysicalDeviceFeatures(physical_device,&features);
    vkGetPhysicalDeviceProperties(physical_device,&properties);
    vkGetPhysicalDeviceMemoryProperties(physical_device,&memory_properties);

    surface=win->CreateSurface(inst);

    {
        uint32_t family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device,&family_count,nullptr);
        family_properties.SetCount(family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device,&family_count,family_properties.GetData());

        {
            supports_present.SetCount(family_count);
            VkBool32 *sp=supports_present.GetData();
            for(uint32_t i=0; i<family_count; i++)
            {
                vkGetPhysicalDeviceSurfaceSupportKHR(physical_device,i,surface,sp);
                ++sp;
            }
        }
    }

    {
        uint32_t format_count;
        if(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device,surface,&format_count,nullptr)==VK_SUCCESS)
        {
            surface_formts.SetCount(format_count);

            if(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device,surface,&format_count,surface_formts.GetData())!=VK_SUCCESS)
                surface_formts.Clear();
        }
    }

    {
        uint32_t mode_count;
        if(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device,surface,&mode_count,nullptr)==VK_SUCCESS)
        {
            present_modes.SetCount(mode_count);
            if(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device,surface,&mode_count,present_modes.GetData())!=VK_SUCCESS)
                present_modes.Clear();
        }
    }

    {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device,surface,&surface_caps);

        VkExtent2D swapchain_extent;
        if(surface_caps.currentExtent.width==0xFFFFFFFF)
        {
            swapchain_extent.width=win->GetWidth();
            swapchain_extent.height=win->GetHeight();

            if(swapchain_extent.width<surface_caps.minImageExtent.width)swapchain_extent.width=surface_caps.minImageExtent.width;else
            if(swapchain_extent.width>surface_caps.maxImageExtent.width)swapchain_extent.width=surface_caps.maxImageExtent.width;

            if(swapchain_extent.height<surface_caps.minImageExtent.height)swapchain_extent.height=surface_caps.minImageExtent.height;else
            if(swapchain_extent.height>surface_caps.maxImageExtent.height)swapchain_extent.height=surface_caps.maxImageExtent.height;
        }
        else
        {
            swapchain_extent=surface_caps.currentExtent;
        }
    }

    CreateDevice();
}

RenderSurface::~RenderSurface()
{
    if(device)
    {
        if(cmd_pool)
            vkDestroyCommandPool(device,cmd_pool,nullptr);

        vkDestroyDevice(device,nullptr);
    }

    if(surface)
        vkDestroySurfaceKHR(instance,surface,nullptr);
}

int RenderSurface::QueueFamilyProperties(VkQueueFlags flag) const
{
    const int count=family_properties.GetCount();

    if(count<=0)
        return(-1);

    VkBool32*sp=supports_present.GetData();
    VkQueueFamilyProperties*fp=family_properties.GetData();
    for(int i=0;i<count;i++)
    {
        if((*sp)
           &&(fp->queueFlags&flag))
            return i;

        ++sp;
        ++fp;
    }

    return -1;
}

bool RenderSurface::CreateCommandPool()
{
    VkCommandPoolCreateInfo cmd_pool_info={};

    cmd_pool_info.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.pNext=NULL;
    cmd_pool_info.queueFamilyIndex=family_index;
    cmd_pool_info.flags=0;

    VkResult res=vkCreateCommandPool(device,&cmd_pool_info,nullptr,&cmd_pool);

    return(res==VK_SUCCESS);
}

CommandBuffer *RenderSurface::CreateCommandBuffer()
{
    if(!cmd_pool)
        return(nullptr);

    VkCommandBufferAllocateInfo cmd={};
    cmd.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd.pNext=nullptr;
    cmd.commandPool=cmd_pool;
    cmd.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd.commandBufferCount=1;

    VkCommandBuffer cmd_buf;

    VkResult res=vkAllocateCommandBuffers(device,&cmd,&cmd_buf);

    if(res!=VK_SUCCESS)
        return(nullptr);

    return(new CommandBuffer(device,cmd_pool,cmd_buf));
}

bool RenderSurface::CreateDevice()
{
    family_index=QueueFamilyProperties(VK_QUEUE_GRAPHICS_BIT);

    if(family_index==-1)
        return(false);

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

    VkResult res=vkCreateDevice(physical_device,&create_info,nullptr,&device);

    if(res!=VK_SUCCESS)
        return(false);

    CreateCommandPool();
    return(true);
}
VK_NAMESPACE_END
