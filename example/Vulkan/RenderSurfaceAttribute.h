#pragma once

#include"VK.h"

VK_NAMESPACE_BEGIN
struct RenderSurfaceAttribute
{
    VkInstance                          instance        =nullptr;
    VkPhysicalDevice                    physical_device =nullptr;
    VkSurfaceKHR                        surface         =nullptr;
    VkSurfaceCapabilitiesKHR            surface_caps;
    VkExtent2D                          swapchain_extent;

    int                                 family_index    =-1;

    List<VkQueueFamilyProperties>       family_properties;
    List<VkBool32>                      supports_present;

    VkPhysicalDeviceFeatures            features;
    VkPhysicalDeviceProperties          properties;
    VkPhysicalDeviceMemoryProperties    memory_properties;

    List<VkSurfaceFormatKHR>            surface_formts;
    List<VkPresentModeKHR>              present_modes;

    VkDevice                            device          =nullptr;
    VkCommandPool                       cmd_pool        =nullptr;

public:

    RenderSurfaceAttribute(VkInstance inst,VkPhysicalDevice pd,VkSurfaceKHR s);
    ~RenderSurfaceAttribute();
};//class RenderSurfaceAttribute
VK_NAMESPACE_END
