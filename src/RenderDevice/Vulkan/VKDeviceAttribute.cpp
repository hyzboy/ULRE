#include<hgl/graph/vulkan/VKDeviceAttribute.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>
#include<hgl/graph/vulkan/VKImageView.h>
#include<iostream>

VK_NAMESPACE_BEGIN

DeviceAttribute::DeviceAttribute(VkInstance inst,const PhysicalDevice *pd,VkSurfaceKHR s)
{
    instance=inst;
    physical_device=pd;
    surface=s;

    Refresh();
}

DeviceAttribute::~DeviceAttribute()
{
    if(pipeline_cache)
        vkDestroyPipelineCache(device,pipeline_cache,nullptr);

    if(desc_pool)
        vkDestroyDescriptorPool(device,desc_pool,nullptr);

    ClearSwapchain();
    
    if(cmd_pool)
        vkDestroyCommandPool(device,cmd_pool,nullptr);

    if(device)
        vkDestroyDevice(device,nullptr);

    if(surface)
        vkDestroySurfaceKHR(instance,surface,nullptr);
}

bool DeviceAttribute::CheckMemoryType(uint32_t typeBits,VkMemoryPropertyFlags properties,uint32_t *typeIndex) const
{
    return physical_device->CheckMemoryType(typeBits,properties,typeIndex);
}

void DeviceAttribute::ClearSwapchain()
{
    SAFE_CLEAR(depth.view);

    if(depth.image)
    {
        vkDestroyImage(device,depth.image,nullptr);
        depth.image=nullptr;
    }

    if(depth.mem)
    {
        vkFreeMemory(device,depth.mem,nullptr);
        depth.mem=nullptr;
    }

    sc_image_views.Clear();

    if(swap_chain)
    {
        vkDestroySwapchainKHR(device,swap_chain,nullptr);
        swap_chain=nullptr;
    }
}

void DeviceAttribute::Refresh()
{
    VkPhysicalDevice pdevice = *physical_device;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pdevice, surface, &surface_caps);

    {
        if (surface_caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        {
            preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        }
        else
        {
            preTransform = surface_caps.currentTransform;
        }
    }

    {
        constexpr VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[4] = { VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                                                      VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                                                                      VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                                                                      VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR };

        for (uint32_t i = 0; i < sizeof(compositeAlphaFlags); i++)
        {
            if (surface_caps.supportedCompositeAlpha & compositeAlphaFlags[i])
            {
                compositeAlpha = compositeAlphaFlags[i];
                break;
            }
        }
    }

    {
        uint32_t format_count;
        if (vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice, surface, &format_count, nullptr) == VK_SUCCESS)
        {
            surface_formts.SetCount(format_count);

            if (vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice, surface, &format_count, surface_formts.GetData()) != VK_SUCCESS)
            {
                surface_formts.Clear();
                format = VK_FORMAT_B8G8R8A8_UNORM;
            }
            else
            {
                VkSurfaceFormatKHR *sf = surface_formts.GetData();

                if (format_count == 1 && sf->format == VK_FORMAT_UNDEFINED)
                    format = VK_FORMAT_B8G8R8A8_UNORM;
                else
                    format = sf->format;
            }
        }
    }

    {
        uint32_t mode_count;
        if (vkGetPhysicalDeviceSurfacePresentModesKHR(pdevice, surface, &mode_count, nullptr) == VK_SUCCESS)
        {
            present_modes.SetCount(mode_count);
            if (vkGetPhysicalDeviceSurfacePresentModesKHR(pdevice, surface, &mode_count, present_modes.GetData()) != VK_SUCCESS)
                present_modes.Clear();
        }
    }

    {
        uint32_t family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &family_count, nullptr);
        family_properties.SetCount(family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &family_count, family_properties.GetData());

        {
            supports_present.SetCount(family_count);
            VkBool32 *sp = supports_present.GetData();
            for (uint32_t i = 0; i < family_count; i++)
            {
                vkGetPhysicalDeviceSurfaceSupportKHR(pdevice, i, surface, sp);
                ++sp;
            }
        }

        {
            VkQueueFamilyProperties *fp = family_properties.GetData();
            VkBool32 *sp = supports_present.GetData();
            for (uint32_t i = 0; i < family_count; i++)
            {
                if (fp->queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    if (graphics_family == ERROR_FAMILY_INDEX)
                        graphics_family = i;

                    if (*sp)
                    {
                        graphics_family = i;
                        present_family = i;
                        break;
                    }
                }

                ++fp;
                ++sp;
            }
        }

        if (present_family == ERROR_FAMILY_INDEX)
        {
            VkBool32 *sp = supports_present.GetData();
            for (uint32_t i = 0; i < family_count; i++)
            {
                if (*sp)
                {
                    present_family = i;
                    break;
                }
                ++sp;
            }
        }
    }
}
VK_NAMESPACE_END
