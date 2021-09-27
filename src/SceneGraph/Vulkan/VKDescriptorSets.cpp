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

        WriteDescriptorSet(VkDescriptorSet desc_set,const uint32_t binding,const VkDescriptorBufferInfo *buf_info,const VkDescriptorType desc_type):WriteDescriptorSet(desc_set,binding,desc_type)
        {
            pImageInfo       = nullptr;
            pBufferInfo      = buf_info;
            pTexelBufferView = nullptr;
        }

        WriteDescriptorSet(VkDescriptorSet desc_set,const uint32_t binding,const VkDescriptorImageInfo *img_info,const VkDescriptorType desc_type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER):WriteDescriptorSet(desc_set,binding,desc_type)
        {            
            pImageInfo      = img_info;
            pBufferInfo     = nullptr;
            pTexelBufferView= nullptr;
        }
    };//struct WriteDescriptorSet

    struct DescriptorBufferInfo:public VkDescriptorBufferInfo
    {
    public:

        DescriptorBufferInfo(const GPUBuffer *buf,const VkDeviceSize off,const VkDeviceSize rng)
        {
            buffer=buf->GetBuffer();
            offset=off;
            range=rng;
        }
    };//struct DescriptorBufferInfo:public VkDescriptorBufferInfo

    struct DescriptorImageInfo:public VkDescriptorImageInfo
    {
    public:

        DescriptorImageInfo(Texture *tex,Sampler *sam)
        {
            sampler=*sam;
            imageView=tex->GetVulkanImageView();
            imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
    };//struct DescriptorImageInfo:public VkDescriptorImageInfo
}//namespace

void DescriptorSets::Clear()
{
    buffer_list.ClearData();
    image_list.ClearData();
    wds_list.ClearData();
    binded_sets.ClearData();
    is_dirty=true;
}

bool DescriptorSets::BindUBO(const int binding,const GPUBuffer *buf,bool dynamic)
{
    if(binding<0||!buf)
        return(false);

    if(binded_sets.IsMember(binding))return(false);

    const VkDescriptorType desc_type=dynamic?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    wds_list.Add(WriteDescriptorSet(desc_set,binding,buf->GetBufferInfo(),desc_type));

    binded_sets.Add(binding);
    is_dirty=true;
    return(true);
}

bool DescriptorSets::BindUBO(const int binding,const GPUBuffer *buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic)
{
    if(binding<0||!buf)
        return(false);

    if(binded_sets.IsMember(binding))return(false);

    DescriptorBufferInfo *buf_info=new DescriptorBufferInfo(buf,offset,range);

    buffer_list.Add(buf_info);

    const VkDescriptorType desc_type=dynamic?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    
    wds_list.Add(WriteDescriptorSet(desc_set,binding,buf_info,desc_type));

    binded_sets.Add(binding);
    is_dirty=true;
    return(true);
}

bool DescriptorSets::BindSSBO(const int binding,const GPUBuffer *buf,bool dynamic)
{
    if(binding<0||!buf)
        return(false);

    if(binded_sets.IsMember(binding))return(false);

    const VkDescriptorType desc_type=dynamic?VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    wds_list.Add(WriteDescriptorSet(desc_set,binding,buf->GetBufferInfo(),desc_type));

    binded_sets.Add(binding);
    is_dirty=true;
    return(true);
}

bool DescriptorSets::BindSSBO(const int binding,const GPUBuffer *buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic)
{
    if(binding<0||!buf)
        return(false);

    if(binded_sets.IsMember(binding))return(false);

    DescriptorBufferInfo *buf_info=new DescriptorBufferInfo(buf,offset,range);

    buffer_list.Add(buf_info);

    const VkDescriptorType desc_type=dynamic?VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    
    wds_list.Add(WriteDescriptorSet(desc_set,binding,buf_info,desc_type));

    binded_sets.Add(binding);
    is_dirty=true;
    return(true);
}

bool DescriptorSets::BindSampler(const int binding,Texture *tex,Sampler *sampler)
{
    if(binding<0||!tex||!sampler)
        return(false);

    if(binded_sets.IsMember(binding))return(false);

    DescriptorImageInfo *image_info=new DescriptorImageInfo(tex,sampler);

    image_list.Add(image_info);

    wds_list.Add(WriteDescriptorSet(desc_set,binding,image_info));

    binded_sets.Add(binding);
    is_dirty=true;
    return(true);
}

bool DescriptorSets::BindInputAttachment(const int binding,Texture *tex)
{
    if(binding<0||!tex)
        return(false);

    if(binded_sets.IsMember(binding))return(false);

    DescriptorImageInfo *image_info=new DescriptorImageInfo(tex,nullptr);
    
    image_list.Add(image_info);

    wds_list.Add(WriteDescriptorSet(desc_set,binding,image_info,VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT));

    binded_sets.Add(binding);
    is_dirty=true;
    return(true);
}

void DescriptorSets::Update()
{
    if(!is_dirty)return;

    vkUpdateDescriptorSets(device,wds_list.GetCount(),wds_list.GetData(),0,nullptr);
    is_dirty=false;
}
VK_NAMESPACE_END
