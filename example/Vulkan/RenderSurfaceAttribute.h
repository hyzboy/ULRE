#pragma once

#include"VK.h"

VK_NAMESPACE_BEGIN

constexpr uint32_t ERROR_FAMILY_INDEX=UINT32_MAX;

struct RenderSurfaceAttribute
{
    VkInstance                          instance        =nullptr;
    VkPhysicalDevice                    physical_device =nullptr;
    VkSurfaceKHR                        surface         =nullptr;
    VkSurfaceCapabilitiesKHR            surface_caps;
    VkExtent2D                          swapchain_extent;

    uint32_t                            graphics_family =ERROR_FAMILY_INDEX;
    uint32_t                            present_family  =ERROR_FAMILY_INDEX;

    List<VkQueueFamilyProperties>       family_properties;
    List<VkBool32>                      supports_present;

    VkPhysicalDeviceFeatures            features;
    VkPhysicalDeviceProperties          properties;
    VkPhysicalDeviceMemoryProperties    memory_properties;

    List<VkSurfaceFormatKHR>            surface_formts;
    VkFormat                            format;
    List<VkPresentModeKHR>              present_modes;

    VkSurfaceTransformFlagBitsKHR       preTransform;
    VkCompositeAlphaFlagBitsKHR         compositeAlpha  =VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    VkDevice                            device          =nullptr;
    VkCommandPool                       cmd_pool        =nullptr;
    VkSwapchainKHR                      swap_chain      =nullptr;

    List<VkImage>                       sc_images;
    List<VkImageView>                   sc_image_views;

    struct
    {
        VkFormat        format;

        VkImage         image   =nullptr;
        VkDeviceMemory  mem     =nullptr;
        VkImageView     view    =nullptr;
    }depth;

public:

    RenderSurfaceAttribute(VkInstance inst,VkPhysicalDevice pd,VkSurfaceKHR s);
    ~RenderSurfaceAttribute();

    bool CheckMemoryType(uint32_t,VkFlags,uint32_t *);
};//class RenderSurfaceAttribute
VK_NAMESPACE_END
