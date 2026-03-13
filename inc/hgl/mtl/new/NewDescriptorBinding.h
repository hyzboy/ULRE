#pragma once

#include<hgl/mtl/new/NewDescriptorSetType.h>
#include<vulkan/vulkan.h>

namespace hgl::graph{

class NewDescriptorBinding
{
public:

    static void BindPerScene   (VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet set);
    static void BindPerView    (VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet set);
    static void BindPerMaterial(VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet set);
    static void BindPerDraw    (VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet set);
};

}//namespace hgl::graph
