#include<hgl/graph/VKDeviceAttribute.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKImageView.h>
#include<hgl/graph/VKTexture.h>
#include<iostream>

VK_NAMESPACE_BEGIN
void SavePipelineCacheData(VkDevice device,VkPipelineCache cache,const VkPhysicalDeviceProperties &pdp);

GPUDeviceAttribute::GPUDeviceAttribute(VulkanInstance *inst,const GPUPhysicalDevice *pd,VkSurfaceKHR s)
{
    instance=inst;
    physical_device=pd;
    surface=s;

    RefreshSurfaceCaps();
    GetSurfacePresentMode();
    GetQueueFamily();
}

GPUDeviceAttribute::~GPUDeviceAttribute()
{
#ifdef _DEBUG
    if(debug_maker)
        delete debug_maker;
    if(debug_utils)
        delete debug_utils;
#endif//_DEBUG
    
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
        instance->DestroySurface(surface);
}

int GPUDeviceAttribute::GetMemoryType(uint32_t typeBits,VkMemoryPropertyFlags properties) const
{
    return physical_device->GetMemoryType(typeBits,properties);
}

void GPUDeviceAttribute::RefreshSurfaceCaps()
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
        constexpr VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[4]=
        {
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR 
        };

        for(auto flags:compositeAlphaFlags)
            if (surface_caps.supportedCompositeAlpha & flags)
            {
                compositeAlpha = flags;
                break;
            }
    }
}

void GPUDeviceAttribute::GetSurfacePresentMode()
{
    uint32_t mode_count;

    VkPhysicalDevice pdevice = *physical_device;

    if (!vkGetPhysicalDeviceSurfacePresentModesKHR(pdevice, surface, &mode_count, nullptr) == VK_SUCCESS)
        return;
    
    present_modes.SetCount(mode_count);

    if (vkGetPhysicalDeviceSurfacePresentModesKHR(pdevice, surface, &mode_count, present_modes.GetData()) != VK_SUCCESS)
        present_modes.Clear();
}

void GPUDeviceAttribute::GetQueueFamily()
{
    VkPhysicalDevice pdevice = *physical_device;

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
            if(fp->queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                if(compute_family==ERROR_FAMILY_INDEX)
                    compute_family=i;
            }

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
VK_NAMESPACE_END
