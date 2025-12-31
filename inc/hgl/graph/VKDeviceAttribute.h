#pragma once

#include<hgl/graph/VKInstance.h>
#include<hgl/graph/VKTexture.h>

#ifdef _DEBUG
#include<hgl/graph/VKDebugUtils.h>
#endif//_DEBUG

VK_NAMESPACE_BEGIN
struct VulkanDevAttr
{
    VulkanInstance *                    instance        =nullptr;
    const VulkanPhyDevice *             physical_device =nullptr;
    
    VkPhysicalDeviceDriverPropertiesKHR driver_properties;

    VulkanSurface *                     surface         =nullptr;

    VkQueue                             graphics_queue  =VK_NULL_HANDLE;
    VkQueue                             present_queue   =VK_NULL_HANDLE;

    VkSurfaceFormatKHR                  surface_format;

    bool                                uint8_index_type    =false;
    bool                                uint32_index_type   =false;
    bool                                wide_lines          =false;

    VkDevice                            device          =VK_NULL_HANDLE;
    VkCommandPool                       cmd_pool        =VK_NULL_HANDLE;

    VkDescriptorPool                    desc_pool       =VK_NULL_HANDLE;

    VkPipelineCache                     pipeline_cache  =VK_NULL_HANDLE;

#ifdef _DEBUG
    DebugUtils *                        debug_utils     =nullptr;
#endif//_DEBUG

public:

    VulkanDevAttr(VulkanInstance *inst,const VulkanPhyDevice *pd,VulkanSurface *s);
    ~VulkanDevAttr();

    int GetMemoryType(uint32_t typeBits,VkMemoryPropertyFlags properties) const;

public:

    template<typename T>
    T *GetDeviceProc(const char *name)
    {
        return instance->GetDeviceProc<T>(device,name);
    }
};//class VulkanDevAttr
VK_NAMESPACE_END
