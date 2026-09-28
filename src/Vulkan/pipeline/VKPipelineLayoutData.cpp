#include<hgl/vk/VKDevice.h>
#include<hgl/graph/ShaderBufferSources.h>

namespace hgl::graph{
// BDA 终态（S3：Scene 集亦退场后）：描述符集只剩 Bindless(0) 一个设备级全局集，
// 全材质共享同一 pipeline layout——不再 per-material 创建（desc_manager 时代遗留）。
// 单例由 ShaderProgramManager 惰建缓存；材质只持裸句柄、不拥有。
VkPipelineLayout VulkanDevice::CreateGlobalPipelineLayout(VkDescriptorSetLayout bindless_layout)
{
    VkDescriptorSetLayout dsl[DESCRIPTOR_SET_TYPE_COUNT]{};

    // Bindless（唯一集合）：layout 由 BindlessTextureManager 提供，GraphicsContext::Init 保证非空。
    dsl[int(DescriptorSetType::Bindless)] = bindless_layout;

    PipelineLayoutCreateInfo pPipelineLayoutCreateInfo;
    pPipelineLayoutCreateInfo.setLayoutCount            = DESCRIPTOR_SET_TYPE_COUNT;
    pPipelineLayoutCreateInfo.pSetLayouts               = dsl;

    // RootAddresses：push constant 承载「全局地址表基址 + 本批表地址」（80B = 9×uint64 +
    // 2×uint32，sizeof(RootAddresses) 为准；容量口径以 ShaderBufferSources.h 的真源与
    // 128B 硬顶为界）。全局 / 长期有效的地址都在表内，见 RootAddressPush.h。
    // stage=Mesh|Fragment|Compute——mesh 读 MeshDrawParams/L2W/L2WIndex/文本表，
    // FS 读 mtl_data_addrs，compute 管线复用同一全局 layout 时按需读全局表地址。
    // IndirectMeshDraw 的 per-draw 段偏移仍走参数表 rows[gl_DrawID]
    // 查表（push constant 放的是表地址，不是 per-draw 数据——一次 vkCmdDrawMeshTasksIndirectEXT
    // 内 N 命令共享同一份，合法）。
    const VkPushConstantRange root_address_range =
    {
        VkShaderStageFlags(hgl::graph::kMeshFragment | VK_SHADER_STAGE_COMPUTE_BIT),
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
