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

    uint32_t                            graphics_family_index = 0;
    uint32_t                            transfer_family_index = 0;

    VkQueue                             graphics_queue  =VK_NULL_HANDLE;
    VkQueue                             present_queue   =VK_NULL_HANDLE;
    VkQueue                             transfer_queue  =VK_NULL_HANDLE;

    VkSurfaceFormatKHR                  surface_format;

    bool                                uint8_index_type    =false;
    bool                                uint32_index_type   =false;
    bool                                wide_lines          =false;
    bool                                use_descriptor_buffer =false;

    VkDevice                            device          =VK_NULL_HANDLE;
    VkCommandPool                       cmd_pool        =VK_NULL_HANDLE;
    VkCommandPool                       transfer_cmd_pool =VK_NULL_HANDLE;

    VkDescriptorPool                    desc_pool       =VK_NULL_HANDLE;

    VkPipelineCache                     pipeline_cache  =VK_NULL_HANDLE;

    // 扩展函数指针（设备创建后经 vkGetDeviceProcAddr 加载一次，避免每次调用查询）
    PFN_vkCmdDrawMeshTasksEXT           cmd_draw_mesh_tasks =nullptr;
    PFN_vkCmdDrawMeshTasksIndirectEXT   cmd_draw_mesh_tasks_indirect =nullptr;
    PFN_vkCmdDrawMeshTasksIndirectCountEXT cmd_draw_mesh_tasks_indirect_count =nullptr;

    // EDS 1/2/3 动态状态（Vulkan 1.3/1.4 核心，但 EXT 函数符号非 loader 静态导出——
    // 统一 vkGetDeviceProcAddr 加载一次，与 mesh shader 函数同模式）
    PFN_vkCmdSetCullModeEXT             cmd_set_cull_mode =nullptr;
    PFN_vkCmdSetDepthTestEnableEXT      cmd_set_depth_test_enable =nullptr;
    PFN_vkCmdSetDepthWriteEnableEXT     cmd_set_depth_write_enable =nullptr;
    PFN_vkCmdSetDepthCompareOpEXT       cmd_set_depth_compare_op =nullptr;
    PFN_vkCmdSetColorBlendEnableEXT     cmd_set_color_blend_enable =nullptr;
    PFN_vkCmdSetColorBlendEquationEXT   cmd_set_color_blend_equation =nullptr;
    PFN_vkCmdSetColorWriteMaskEXT       cmd_set_color_write_mask =nullptr;
    PFN_vkCmdSetPolygonModeEXT          cmd_set_polygon_mode =nullptr;
    PFN_vkCmdSetAlphaToCoverageEnableEXT cmd_set_alpha_to_coverage_enable =nullptr;

    // Push Descriptor（Vulkan 1.4 核心 / VK_KHR_push_descriptor）
    PFN_vkCmdPushDescriptorSet          cmd_push_descriptor_set =nullptr;

    // Synchronization2（Vulkan 1.3 核心 / VK_KHR_synchronization2）
    PFN_vkCmdPipelineBarrier2           cmd_pipeline_barrier2 =nullptr;
    PFN_vkQueueSubmit2                  queue_submit2 =nullptr;

    // Descriptor Buffer（VK_EXT_descriptor_buffer）
    PFN_vkGetDescriptorSetLayoutSizeEXT          get_descriptor_set_layout_size =nullptr;
    PFN_vkGetDescriptorSetLayoutBindingOffsetEXT get_descriptor_set_layout_binding_offset =nullptr;
    PFN_vkGetDescriptorEXT                       get_descriptor =nullptr;
    PFN_vkCmdBindDescriptorBuffersEXT            cmd_bind_descriptor_buffers =nullptr;
    PFN_vkCmdSetDescriptorBufferOffsetsEXT       cmd_set_descriptor_buffer_offsets =nullptr;

#ifdef _DEBUG
    DebugUtils *                        debug_utils     =nullptr;
#endif//_DEBUG

public:

    VulkanDevAttr(VulkanInstance *inst,const VulkanPhyDevice *pd,VulkanSurface *s);
    ~VulkanDevAttr();

    int GetMemoryType(uint32_t typeBits,VkMemoryPropertyFlags properties) const;

public:

    template<typename T>
    T GetDeviceProc(const char *name)
    {
        return instance->GetDeviceProc<T>(device,name);
    }
};//class VulkanDevAttr
}//namespace hgl::graph

