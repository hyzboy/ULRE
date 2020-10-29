#include<hgl/graph/VKDescriptorSets.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKSampler.h>

VK_NAMESPACE_BEGIN
namespace
{
    struct WriteDescriptorSet:public vkstruct<VkWriteDescriptorSet,VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET>
    {
    public:

        WriteDescriptorSet(VkDescriptorSet desc_set,const uint32_t binding,const VkDescriptorType desc_type)
        {            
            dstSet          = desc_set;
            dstBinding      = binding;
            dstArrayElement = 0;
            descriptorCount = 1;
            descriptorType  = desc_type;
        }

        WriteDescriptorSet(VkDescriptorSet desc_set,const uint32_t binding,const VkDescriptorBufferInfo *buf_info,const VkDescriptorType desc_type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER):WriteDescriptorSet(desc_set,binding,desc_type)
        {            
            pImageInfo       = nullptr;
            pBufferInfo      = buf_info;
            pTexelBufferView = nullptr;
        }

        WriteDescriptorSet(VkDescriptorSet desc_set,const uint32_t binding,const VkDescriptorImageInfo *img_info):WriteDescriptorSet(desc_set,binding,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        {            
            pImageInfo      = img_info;
            pBufferInfo     = nullptr;
            pTexelBufferView= nullptr;
        }
    };//struct WriteDescriptorSet
}//namespace

void DescriptorSets::Clear()
{
    buffer_list.ClearData();
    image_list.ClearData();
    wds_list.ClearData();
}

bool DescriptorSets::BindUBO(const int binding,const GPUBuffer *buf)
{
    if(binding<0||!buf)
        return(false);

    WriteDescriptorSet wds(desc_set,binding,buf->GetBufferInfo());

    wds_list.Add(wds);
    return(true);
}

bool DescriptorSets::BindUBO(const int binding,const GPUBuffer *buf,const VkDeviceSize offset,const VkDeviceSize range)
{
    if(binding<0||!buf)
        return(false);

    VkDescriptorBufferInfo *buf_info=new VkDescriptorBufferInfo;

    buf_info->buffer=buf->GetBuffer();
    buf_info->offset=offset;
    buf_info->range=range;

    buffer_list.Add(buf_info);

    WriteDescriptorSet wds(desc_set,binding,buf_info);

    wds_list.Add(wds);
    return(true);
}

bool DescriptorSets::BindUBODynamic(const int binding,const GPUBuffer *buf)
{
    if(binding<0||!buf)
        return(false);
        
    WriteDescriptorSet wds(desc_set,binding,buf->GetBufferInfo(),VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);

    wds_list.Add(wds);
    return(true);
}

bool DescriptorSets::BindSampler(const int binding,Texture *tex,Sampler *sampler)
{
    if(binding<0||!tex||!sampler)
        return(false);

    VkDescriptorImageInfo *image_info=new VkDescriptorImageInfo;

    image_info->imageView    =tex->GetVulkanImageView();
//          image_info.imageLayout  =tex->GetImageLayout();
    image_info->imageLayout  =VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info->sampler      =*sampler;

    image_list.Add(image_info);

    WriteDescriptorSet wds(desc_set,binding,image_info);

    wds_list.Add(wds);
    return(true);
}

void DescriptorSets::Update()
{
    vkUpdateDescriptorSets(device,wds_list.GetCount(),wds_list.GetData(),0,nullptr);
}
VK_NAMESPACE_END
