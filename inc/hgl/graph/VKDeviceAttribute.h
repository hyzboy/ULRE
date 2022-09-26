#pragma once

#include<hgl/graph/VKInstance.h>
#include<hgl/graph/VKTexture.h>

#ifdef _DEBUG
#include<hgl/graph/VKDebugMaker.h>
#include<hgl/graph/VKDebugUtils.h>
#endif//_DEBUG

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
    uint32_t                            compute_family  =ERROR_FAMILY_INDEX;

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
    
#ifdef _DEBUG
    DebugMaker *                        debug_maker     =nullptr;
    DebugUtils *                        debug_utils     =nullptr;
#endif//_DEBUG

public:

    GPUDeviceAttribute(VulkanInstance *inst,const GPUPhysicalDevice *pd,VkSurfaceKHR s);
    ~GPUDeviceAttribute();

    int GetMemoryType(uint32_t typeBits,VkMemoryPropertyFlags properties) const;

    void RefreshSurfaceCaps();

private:

    void GetSurfaceFormatList();
    void GetSurfacePresentMode();
    void GetQueueFamily();

public:

    template<typename T>
    T *GetDeviceProc(const char *name)
    {
        return instance->GetDeviceProc<T>(device,name);
    }
};//class GPUDeviceAttribute
VK_NAMESPACE_END
