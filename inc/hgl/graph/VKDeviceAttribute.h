﻿#pragma once

#include<hgl/graph/VKInstance.h>
#include<hgl/graph/VKTexture.h>

VK_NAMESPACE_BEGIN

constexpr uint32_t ERROR_FAMILY_INDEX=UINT32_MAX;

struct GPUDeviceAttribute
{
    VulkanInstance *                    instance        =nullptr;
    const GPUPhysicalDevice *           physical_device =nullptr;
    
    VkPhysicalDeviceDriverPropertiesKHR driver_properties;

    VkSurfaceKHR                        surface         =VK_NULL_HANDLE;
    VkSurfaceCapabilitiesKHR            surface_caps;

    uint32_t                            graphics_family =ERROR_FAMILY_INDEX;
    uint32_t                            present_family  =ERROR_FAMILY_INDEX;

    VkQueue                             graphics_queue  =VK_NULL_HANDLE;
    VkQueue                             present_queue   =VK_NULL_HANDLE;

    List<VkQueueFamilyProperties>       family_properties;
    List<VkBool32>                      supports_present;

    List<VkSurfaceFormatKHR>            surface_formats_list;
    VkSurfaceFormatKHR                  surface_format;
    List<VkPresentModeKHR>              present_modes;

    VkSurfaceTransformFlagBitsKHR       preTransform;
    VkCompositeAlphaFlagBitsKHR         compositeAlpha  =VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    VkDevice                            device          =VK_NULL_HANDLE;
    VkCommandPool                       cmd_pool        =VK_NULL_HANDLE;

    VkDescriptorPool                    desc_pool       =VK_NULL_HANDLE;

    VkPipelineCache                     pipeline_cache  =VK_NULL_HANDLE;

public:

    GPUDeviceAttribute(VulkanInstance *inst,const GPUPhysicalDevice *pd,VkSurfaceKHR s);
    ~GPUDeviceAttribute();

    bool CheckMemoryType(uint32_t typeBits,VkMemoryPropertyFlags properties,uint32_t *typeIndex) const;

    void Refresh();

public:

    template<typename T>
    T *GetDeviceProc(const char *name)
    {
        return instance->GetDeviceProc<T>(device,name);
    }
};//class GPUDeviceAttribute
VK_NAMESPACE_END
