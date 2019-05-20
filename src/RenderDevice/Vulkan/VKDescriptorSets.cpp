#include<hgl/graph/vulkan/VKDescriptorSets.h>
#include<hgl/graph/vulkan/VKDevice.h>

VK_NAMESPACE_BEGIN
bool DescriptorSets::UpdateUBO(const uint32_t binding,const VkDescriptorBufferInfo *buf_info)
{
    VkWriteDescriptorSet writeDescriptorSet = {};

    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = desc_set;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = buf_info;
    writeDescriptorSet.dstBinding = binding;

    vkUpdateDescriptorSets(*device,1,&writeDescriptorSet,0,nullptr);
    return(true);
}

bool DescriptorSets::UpdateSampler(const uint32_t binding,const VkDescriptorImageInfo *image_info)
{
    VkWriteDescriptorSet writeDescriptorSet = {};

    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = desc_set;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.pImageInfo = image_info;
    writeDescriptorSet.dstBinding = binding;

    vkUpdateDescriptorSets(*device,1,&writeDescriptorSet,0,nullptr);
    return(true);
}
VK_NAMESPACE_END
