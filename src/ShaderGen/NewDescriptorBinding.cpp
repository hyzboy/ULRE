/// NewDescriptorBinding.cpp — 新 4-Set 绑定适配器

#include<hgl/mtl/new/NewDescriptorBinding.h>

namespace hgl::graph{

static void BindSet(VkCommandBuffer cmd, VkPipelineLayout layout, uint32_t set_index, VkDescriptorSet set)
{
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            layout,
                            set_index,     // firstSet
                            1,             // descriptorSetCount
                            &set,
                            0,             // dynamicOffsetCount
                            nullptr);
}

void NewDescriptorBinding::BindPerScene(VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet set)
{
    BindSet(cmd, layout, static_cast<uint32_t>(NewDescriptorSetType::PerScene), set);
}

void NewDescriptorBinding::BindPerView(VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet set)
{
    BindSet(cmd, layout, static_cast<uint32_t>(NewDescriptorSetType::PerView), set);
}

void NewDescriptorBinding::BindPerMaterial(VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet set)
{
    BindSet(cmd, layout, static_cast<uint32_t>(NewDescriptorSetType::PerMaterial), set);
}

void NewDescriptorBinding::BindPerDraw(VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet set)
{
    BindSet(cmd, layout, static_cast<uint32_t>(NewDescriptorSetType::PerDraw), set);
}

}//namespace hgl::graph
