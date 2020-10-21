#include<hgl/graph/VKDescriptorSets.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKSampler.h>

VK_NAMESPACE_BEGIN
void DescriptorSets::Clear()
{
    wds_list.ClearData();
    desc_image_info.ClearData();
}

bool DescriptorSets::BindUBO(const int binding,const Buffer *buf)
{
    if(binding<0||!buf)
        return(false);

    WriteDescriptorSet wds;

    wds.dstSet           = desc_set;
    wds.dstBinding       = binding;
    wds.dstArrayElement  = 0;
    wds.descriptorCount  = 1;
    wds.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    wds.pImageInfo       = nullptr;
    wds.pBufferInfo      = buf->GetBufferInfo();
    wds.pTexelBufferView = nullptr;

    wds_list.Add(wds);
    return(true);
}

bool DescriptorSets::BindUBODynamic(const int binding,const Buffer *buf)
{
    if(binding<0||!buf)
        return(false);
        
    WriteDescriptorSet wds;

    wds.dstSet           = desc_set;
    wds.dstBinding       = binding;
    wds.dstArrayElement  = 0;
    wds.descriptorCount  = 1;
    wds.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    wds.pImageInfo       = nullptr;
    wds.pBufferInfo      = buf->GetBufferInfo();
    wds.pTexelBufferView = nullptr;

    wds_list.Add(wds);
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
    
    WriteDescriptorSet wds;

    wds.dstSet           = desc_set;
    wds.dstBinding       = binding;
    wds.dstArrayElement  = 0;
    wds.descriptorCount  = 1;
    wds.descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wds.pImageInfo       = image_info;
    wds.pBufferInfo      = nullptr;
    wds.pTexelBufferView = nullptr;

    wds_list.Add(wds);
    return(true);
}

void DescriptorSets::Update()
{
    vkUpdateDescriptorSets(device,wds_list.GetCount(),wds_list.GetData(),0,nullptr);
}
VK_NAMESPACE_END
