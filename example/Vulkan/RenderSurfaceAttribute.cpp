#include"RenderSurfaceAttribute.h"

VK_NAMESPACE_BEGIN
RenderSurfaceAttribute::RenderSurfaceAttribute(VkInstance inst,VkPhysicalDevice pd,VkSurfaceKHR s)
{
    instance=inst;
    physical_device=pd;
    surface=s;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device,surface,&surface_caps);

    vkGetPhysicalDeviceFeatures(physical_device,&features);
    vkGetPhysicalDeviceProperties(physical_device,&properties);
    vkGetPhysicalDeviceMemoryProperties(physical_device,&memory_properties);

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
        uint32_t family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device,&family_count,nullptr);
        family_properties.SetCount(family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device,&family_count,family_properties.GetData());

        VkQueueFamilyProperties *fp=family_properties.GetData();

        supports_present.SetCount(family_count);
        VkBool32 *sp=supports_present.GetData();
        for(uint32_t i=0; i<family_count; i++)
        {
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device,i,surface,sp);

            if(family_index==-1)
            {
                if(*sp&&(fp->queueFlags&VK_QUEUE_GRAPHICS_BIT))
                    family_index=i;
            }

            ++fp;
            ++sp;
        }
    }
}

RenderSurfaceAttribute::~RenderSurfaceAttribute()
{
    if(cmd_pool)
        vkDestroyCommandPool(device,cmd_pool,nullptr);

    if(device)
        vkDestroyDevice(device,nullptr);

    if(surface)
        vkDestroySurfaceKHR(instance,surface,nullptr);
}
VK_NAMESPACE_END
