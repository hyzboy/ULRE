#include<hgl/vk/VKDevice.h>
#include<hgl/graph/ShaderBufferSources.h>

namespace hgl::graph{
namespace
{
    static VkDescriptorSetLayout CreateEmptyDescriptorSetLayout(VkDevice device)
    {
        VkDescriptorSetLayoutCreateInfo empty_ci{};
        empty_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        empty_ci.bindingCount = 0;

        VkDescriptorSetLayout empty_layout = VK_NULL_HANDLE;
        if(vkCreateDescriptorSetLayout(device, &empty_ci, nullptr, &empty_layout) != VK_SUCCESS)
            return VK_NULL_HANDLE;

        return empty_layout;
    }
}

// BDA 终态（A6-2b/b3 后）：描述符集只剩 Scene(0)/Bindless(1) 两个设备级全局集，
// 全材质共享同一 pipeline layout——不再 per-material 创建（desc_manager 时代遗留）。
// 单例由 ShaderProgramManager 惰建缓存；材质只持裸句柄、不拥有。
VkPipelineLayout VulkanDevice::CreateGlobalPipelineLayout(VkDescriptorSetLayout bindless_layout,
                                                          VkDescriptorSetLayout scene_layout)
{
    VkDescriptorSetLayout dsl[DESCRIPTOR_SET_TYPE_COUNT]{};

    if(scene_layout != VK_NULL_HANDLE)
    {
        dsl[int(DescriptorSetType::Scene)] = scene_layout;
    }
    else
    {
        // 全局 Scene 集未就绪的占位空 layout（正常 Init 顺序下不出现；
        // 仅建一次——共享单例路径，随设备生命周期存续）。
        dsl[int(DescriptorSetType::Scene)] = CreateEmptyDescriptorSetLayout(attr->device);
        if(dsl[int(DescriptorSetType::Scene)] == VK_NULL_HANDLE)
            return VK_NULL_HANDLE;
    }

    // Bindless（Set 1）：layout 由 BindlessTextureManager 提供，GraphicsContext::Init 保证非空。
    dsl[int(DescriptorSetType::Bindless)] = bindless_layout;

    PipelineLayoutCreateInfo pPipelineLayoutCreateInfo;
    pPipelineLayoutCreateInfo.setLayoutCount            = DESCRIPTOR_SET_TYPE_COUNT;
    pPipelineLayoutCreateInfo.pSetLayouts               = dsl;

    // RootAddresses：push constant 承载 7 张全局表设备地址（56B，HGL_ROOT_ADDRESSES_FIELD_LIST）。
    // stage=Mesh|Fragment（kMeshFragment）——mesh 读 MeshDrawParams/L2W/L2WIndex/文本表，
    // FS 读 mtl_data_addrs。IndirectMeshDraw 的 per-draw 段偏移仍走参数表 rows[gl_DrawID]
    // 查表（push constant 放的是表地址，不是 per-draw 数据——一次 vkCmdDrawMeshTasksIndirectEXT
    // 内 N 命令共享同一份，合法）。
    const VkPushConstantRange root_address_range =
    {
        VkShaderStageFlags(hgl::graph::kMeshFragment),
        0,
        uint32_t(sizeof(hgl::graph::mtl::RootAddresses))
    };
    pPipelineLayoutCreateInfo.pushConstantRangeCount    = 1;
    pPipelineLayoutCreateInfo.pPushConstantRanges       = &root_address_range;

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    if(vkCreatePipelineLayout(attr->device,&pPipelineLayoutCreateInfo,nullptr,&pipeline_layout)!=VK_SUCCESS)
        return VK_NULL_HANDLE;

    return pipeline_layout;
}
}//namespace hgl::graph
