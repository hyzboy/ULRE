#include<hgl/graph/VKDeviceAttribute.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKImageView.h>
#include<hgl/graph/VKTexture.h>
#include<iostream>

VK_NAMESPACE_BEGIN
void SavePipelineCacheData(VkDevice device,VkPipelineCache cache,const VkPhysicalDeviceProperties &pdp);

GPUDeviceAttribute::GPUDeviceAttribute(VkInstance inst,const GPUPhysicalDevice *pd,VkSurfaceKHR s)
{
    instance=inst;
    physical_device=pd;
    surface=s;

    Refresh();
}

GPUDeviceAttribute::~GPUDeviceAttribute()
{
    if(pipeline_cache)
    {
        SavePipelineCacheData(device,pipeline_cache,physical_device->GetProperties());
        vkDestroyPipelineCache(device,pipeline_cache,nullptr);
    }

    if(desc_pool)
        vkDestroyDescriptorPool(device,desc_pool,nullptr);

    if(cmd_pool)
        vkDestroyCommandPool(device,cmd_pool,nullptr);

    if(device)
        vkDestroyDevice(device,nullptr);

    if(surface)
        vkDestroySurfaceKHR(instance,surface,nullptr);
}

bool GPUDeviceAttribute::CheckMemoryType(uint32_t typeBits,VkMemoryPropertyFlags properties,uint32_t *typeIndex) const
{
    return physical_device->CheckMemoryType(typeBits,properties,typeIndex);
}

void GPUDeviceAttribute::Refresh()
{
    VkPhysicalDevice pdevice = *physical_device;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pdevice, surface, &surface_caps);

    {
        if (surface_caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        else
            preTransform = surface_caps.currentTransform;
    }

    {
        constexpr VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[4]={VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                                                      VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                                                                      VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                                                                      VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR };

        for(auto flags:compositeAlphaFlags)
            if (surface_caps.supportedCompositeAlpha & flags)
            {
                compositeAlpha = flags;
                break;
            }
    }

    {
        uint32_t format_count;

        surface_format.format       = VK_FORMAT_B8G8R8A8_SRGB;
        surface_format.colorSpace   = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        if (vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice, surface, &format_count, nullptr) == VK_SUCCESS)
        {
            surface_formats_list.SetCount(format_count);

            if (vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice, surface, &format_count, surface_formats_list.GetData()) != VK_SUCCESS)
            {
                surface_formats_list.Clear();
            }
            else
            {
                VkSurfaceFormatKHR *sf = surface_formats_list.GetData();

                if (format_count>1)
                {
                    surface_format.format=VK_FORMAT_UNDEFINED;

                    for(uint32_t i=0;i<format_count;i++)
                    {
                        if(sf->format>surface_format.format)
                            surface_format=*sf;

                        ++sf;
                    }
                }
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
