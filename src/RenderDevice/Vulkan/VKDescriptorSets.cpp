#include<hgl/graph/vulkan/VKDescriptorSets.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKSampler.h>

VK_NAMESPACE_BEGIN
void DescriptorSets::Clear()
{
    write_desc_sets.ClearData();
    desc_image_info.ClearData();
}

bool DescriptorSets::BindUBO(const uint32_t binding,const VkDescriptorBufferInfo *buf_info)
{
    VkWriteDescriptorSet writeDescriptorSet = {};

    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = desc_set;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = buf_info;
    writeDescriptorSet.dstBinding = binding;

    write_desc_sets.Add(writeDescriptorSet);
    return(true);
}

bool DescriptorSets::BindSampler(const uint32_t binding,Texture *tex,Sampler *sampler)
{
    if(!tex||!sampler)
        return(false);

    VkDescriptorImageInfo *image_info=desc_image_info.Add();
    image_info->imageView    =*tex;
    image_info->imageLayout  =*tex;
    image_info->sampler      =*sampler;

    VkWriteDescriptorSet writeDescriptorSet = {};

    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = desc_set;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.pImageInfo = image_info;
    writeDescriptorSet.dstBinding = binding;

    write_desc_sets.Add(writeDescriptorSet);
    return(true);
}

void DescriptorSets::Update()
{
    vkUpdateDescriptorSets(*device,write_desc_sets.GetCount(),write_desc_sets.GetData(),0,nullptr);
}
VK_NAMESPACE_END
