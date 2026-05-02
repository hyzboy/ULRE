#pragma once

#include<hgl/vk/VKInstance.h>
#include<hgl/vk/VKTexture.h>

#ifdef _DEBUG
#include<hgl/vk/VKDebugUtils.h>
#endif//_DEBUG

namespace hgl::graph{
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
    bool                                mesh_shader_extension =false;
    bool                                mesh_shader_enabled =false;
    bool                                task_shader_enabled =false;

    VkDevice                            device          =VK_NULL_HANDLE;
    VkCommandPool                       cmd_pool        =VK_NULL_HANDLE;

    VkDescriptorPool                    desc_pool       =VK_NULL_HANDLE;

    VkPipelineCache                     pipeline_cache  =VK_NULL_HANDLE;

    PFN_vkCmdBeginRenderingKHR          pfn_vkCmdBeginRenderingKHR = nullptr;
    PFN_vkCmdEndRenderingKHR            pfn_vkCmdEndRenderingKHR   = nullptr;

#ifdef VK_EXT_mesh_shader
    PFN_vkCmdDrawMeshTasksEXT           pfn_vkCmdDrawMeshTasksEXT = nullptr;
    PFN_vkCmdDrawMeshTasksIndirectEXT   pfn_vkCmdDrawMeshTasksIndirectEXT = nullptr;
    PFN_vkCmdDrawMeshTasksIndirectCountEXT pfn_vkCmdDrawMeshTasksIndirectCountEXT = nullptr;
#endif

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
}//namespace hgl::graph
