#include<hgl/graph/vulkan/VKDescriptorSets.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKBuffer.h>
#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKSampler.h>

VK_NAMESPACE_BEGIN
void DescriptorSets::Clear()
{
    write_desc_sets.ClearData();
    desc_image_info.ClearData();
}

bool DescriptorSets::BindUBO(const int binding,const Buffer *buf)
{
    if(binding<0||!buf)
        return(false);

    VkWriteDescriptorSet writeDescriptorSet;

    writeDescriptorSet.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.pNext            = nullptr;
    writeDescriptorSet.dstSet           = desc_set;
    writeDescriptorSet.dstBinding       = binding;
    writeDescriptorSet.dstArrayElement  = 0;
    writeDescriptorSet.descriptorCount  = 1;
    writeDescriptorSet.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pImageInfo       = nullptr;
    writeDescriptorSet.pBufferInfo      = buf->GetBufferInfo();
    writeDescriptorSet.pTexelBufferView = nullptr;

    write_desc_sets.Add(writeDescriptorSet);
    return(true);
}

bool DescriptorSets::BindUBODynamic(const int binding,const Buffer *buf)
{
    if(binding<0||!buf)
        return(false);

    VkWriteDescriptorSet writeDescriptorSet;

    writeDescriptorSet.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.pNext            = nullptr;
    writeDescriptorSet.dstSet           = desc_set;
    writeDescriptorSet.dstBinding       = binding;
    writeDescriptorSet.dstArrayElement  = 0;
    writeDescriptorSet.descriptorCount  = 1;
    writeDescriptorSet.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writeDescriptorSet.pImageInfo       = nullptr;
    writeDescriptorSet.pBufferInfo      = buf->GetBufferInfo();
    writeDescriptorSet.pTexelBufferView = nullptr;

    write_desc_sets.Add(writeDescriptorSet);
    return(true);
}

bool DescriptorSets::BindSampler(const int binding,Texture *tex,Sampler *sampler)
{
    if(binding<0||!tex||!sampler)
        return(false);

    VkDescriptorImageInfo *image_info=new VkDescriptorImageInfo;
    image_info->imageView    =tex->GetVulkanImageView();
    //image_info->imageLayout  =tex->GetImageLayout();
    image_info->imageLayout  =VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info->sampler      =*sampler;

    desc_image_info.Add(image_info);

    VkWriteDescriptorSet writeDescriptorSet;

    writeDescriptorSet.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.pNext            = nullptr;
    writeDescriptorSet.dstSet           = desc_set;
    writeDescriptorSet.dstBinding       = binding;
    writeDescriptorSet.dstArrayElement  = 0;
    writeDescriptorSet.descriptorCount  = 1;
    writeDescriptorSet.descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.pImageInfo       = image_info;
    writeDescriptorSet.pBufferInfo      = nullptr;
    writeDescriptorSet.pTexelBufferView = nullptr;

    write_desc_sets.Add(writeDescriptorSet);
    return(true);
}

void DescriptorSets::Update()
{
    vkUpdateDescriptorSets(*device,write_desc_sets.GetCount(),write_desc_sets.GetData(),0,nullptr);
}
VK_NAMESPACE_END
