#include<hgl/vk/VKDevice.h>

namespace hgl::graph{
// BDA 终态（S3：Scene 集亦退场后）：描述符集只剩 Bindless(0) 一个设备级全局集，
// 全材质共享同一 pipeline layout——不再 per-material 创建（desc_manager 时代遗留）。
// 单例由 ShaderProgramManager 惰建缓存；材质只持裸句柄、不拥有。
//
// push constant 的**布局**（RootAddresses 的大小与 stage 位）属于上层 shader 契约，
// 由调用方传入；本层只按给定范围创建 layout，不再 include SceneGraph 的布局头。
VkPipelineLayout VulkanDevice::CreateGlobalPipelineLayout(VkDescriptorSetLayout bindless_layout,
                                                          const VkPushConstantRange *push_constants,
                                                          uint32_t push_constant_count)
{
    VkDescriptorSetLayout dsl[DESCRIPTOR_SET_TYPE_COUNT]{};

    // Bindless（唯一集合）：layout 由 BindlessTextureManager 提供，GraphicsContext::Init 保证非空。
    dsl[int(DescriptorSetType::Bindless)] = bindless_layout;

    PipelineLayoutCreateInfo pPipelineLayoutCreateInfo;
    pPipelineLayoutCreateInfo.setLayoutCount            = DESCRIPTOR_SET_TYPE_COUNT;
    pPipelineLayoutCreateInfo.pSetLayouts               = dsl;

    pPipelineLayoutCreateInfo.pushConstantRangeCount    = push_constant_count;
    pPipelineLayoutCreateInfo.pPushConstantRanges       = push_constants;

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    if(vkCreatePipelineLayout(attr->device,&pPipelineLayoutCreateInfo,nullptr,&pipeline_layout)!=VK_SUCCESS)
        return VK_NULL_HANDLE;

    return pipeline_layout;
}
}//namespace hgl::graph
