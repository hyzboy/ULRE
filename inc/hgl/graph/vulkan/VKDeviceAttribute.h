#pragma once

#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKImageView.h>

VK_NAMESPACE_BEGIN

constexpr uint32_t ERROR_FAMILY_INDEX=UINT32_MAX;

struct DeviceAttribute
{
    VkInstance                          instance        =nullptr;
    const PhysicalDevice *              physical_device =nullptr;

    VkSurfaceKHR                        surface         =nullptr;
    VkSurfaceCapabilitiesKHR            surface_caps;
    VkExtent2D                          swapchain_extent;

    uint32_t                            graphics_family =ERROR_FAMILY_INDEX;
    uint32_t                            present_family  =ERROR_FAMILY_INDEX;

    VkQueue                             graphics_queue  =nullptr;
    VkQueue                             present_queue   =nullptr;

    List<VkQueueFamilyProperties>       family_properties;
    List<VkBool32>                      supports_present;

    List<VkSurfaceFormatKHR>            surface_formts;
    VkFormat                            format;
    List<VkPresentModeKHR>              present_modes;

    VkSurfaceTransformFlagBitsKHR       preTransform;
    VkCompositeAlphaFlagBitsKHR         compositeAlpha  =VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    VkDevice                            device          =nullptr;
    VkCommandPool                       cmd_pool        =nullptr;
    VkSwapchainKHR                      swap_chain      =nullptr;

    List<VkImage>                       sc_images;
    ObjectList<ImageView>               sc_image_views;

    Texture2D *                         sc_depth        =nullptr;

    VkDescriptorPool                    desc_pool       =nullptr;

    VkPipelineCache                     pipeline_cache  =nullptr;

public:

    DeviceAttribute(VkInstance inst,const PhysicalDevice *pd,VkSurfaceKHR s);
    ~DeviceAttribute();

    bool CheckMemoryType(uint32_t typeBits,VkMemoryPropertyFlags properties,uint32_t *typeIndex) const;

    void ClearSwapchain();
    void Refresh();
};//class DeviceAttribute
VK_NAMESPACE_END
