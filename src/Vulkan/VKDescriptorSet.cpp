#include<hgl/vk/VKDescriptorSet.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKSampler.h>

namespace hgl::graph{
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

        DescriptorBufferInfo(const VkBufferOwner *buf,const VkDeviceSize off,const VkDeviceSize rng)
        {
            buffer=buf->GetBuffer();
            offset=off;
            range=rng;
        }
    };//struct DescriptorBufferInfo:public VkDescriptorBufferInfo

    struct DescriptorImageInfo:public VkDescriptorImageInfo
    {
    public:

        DescriptorImageInfo(Texture *tex)
        {
            sampler = VK_NULL_HANDLE;
            imageView = tex->GetVulkanImageView();
            imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        DescriptorImageInfo(Texture *tex,Sampler *sam)
        {
            sampler=*sam;
            imageView=tex->GetVulkanImageView();
            imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        DescriptorImageInfo(VkImageView iv)
        {
            sampler=VK_NULL_HANDLE;
            imageView=iv;
            imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
    };//struct DescriptorImageInfo:public VkDescriptorImageInfo
}//namespace

void DescriptorSet::SyncWriteDescriptorInfoPointers()
{
    VkDescriptorBufferInfo *buffer_base = vab_list.data();
    VkDescriptorImageInfo *image_base = image_list.data();

    const int write_count = (int)wds_list.size();
    const int buffer_count = (int)vab_list.size();
    const int image_count = (int)image_list.size();

    for (int i = 0; i < write_count; ++i)
    {
        VkWriteDescriptorSet &wds = wds_list[i];

        const int buf_index = (i < (int)wds_buffer_info_indices.size()) ? wds_buffer_info_indices[i] : -1;
        const int img_index = (i < (int)wds_image_info_indices.size()) ? wds_image_info_indices[i] : -1;

        if (buf_index >= 0 && buffer_base && buf_index < buffer_count)
        {
            wds.pBufferInfo = buffer_base + buf_index;
            wds.pImageInfo = nullptr;
            continue;
        }

        if (img_index >= 0 && image_base && img_index < image_count)
        {
            wds.pImageInfo = image_base + img_index;
            wds.pBufferInfo = nullptr;
            continue;
        }

        wds.pBufferInfo = nullptr;
        wds.pImageInfo = nullptr;
    }
}

void DescriptorSet::Clear()
{
    vab_list.clear();
    image_list.clear();
    wds_list.clear();
    wds_buffer_info_indices.clear();
    wds_image_info_indices.clear();
    binded_sets.Clear();
    is_dirty=false;
}

bool DescriptorSet::BindUBO(const int binding,const VkBufferOwner *buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic)
{
    if(binding<0||!buf)
        return(false);

    if(binded_sets.Contains(binding))return(false);

    vab_list.push_back(DescriptorBufferInfo(buf,offset,range));
    const int buf_index=(int)vab_list.size()-1;

    const VkDescriptorType desc_type=dynamic?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    wds_list.push_back(WriteDescriptorSet(desc_set,binding,(const VkDescriptorBufferInfo *)nullptr,desc_type));
    wds_buffer_info_indices.push_back(buf_index);
    wds_image_info_indices.push_back(-1);
    SyncWriteDescriptorInfoPointers();

    binded_sets.Add(binding);
    is_dirty=true;
    return(true);
}

bool DescriptorSet::BindSSBO(const int binding,const VkBufferOwner *buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic)
{
    if(binding<0||!buf)
        return(false);

    if(binded_sets.Contains(binding))return(false);

    vab_list.push_back(DescriptorBufferInfo(buf,offset,range));
    const int buf_index=(int)vab_list.size()-1;

    const VkDescriptorType desc_type=dynamic?VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    wds_list.push_back(WriteDescriptorSet(desc_set,binding,(const VkDescriptorBufferInfo *)nullptr,desc_type));
    wds_buffer_info_indices.push_back(buf_index);
    wds_image_info_indices.push_back(-1);
    SyncWriteDescriptorInfoPointers();

    binded_sets.Add(binding);
    is_dirty=true;
    return(true);
}

bool DescriptorSet::BindUBO(const int binding,const IGPUBuffer *gpu,bool dynamic)
{
    if(binding<0||!gpu)
        return(false);

    if(binded_sets.Contains(binding))return(false);

    const VkDescriptorType desc_type=dynamic?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    vab_list.push_back(gpu->GetDescriptorBufferInfo());
    const int buf_index=(int)vab_list.size()-1;
    wds_list.push_back(WriteDescriptorSet(desc_set,binding,(const VkDescriptorBufferInfo *)nullptr,desc_type));
    wds_buffer_info_indices.push_back(buf_index);
    wds_image_info_indices.push_back(-1);
    SyncWriteDescriptorInfoPointers();

    binded_sets.Add(binding);
    is_dirty=true;
    return(true);
}

bool DescriptorSet::BindSSBO(const int binding,const IGPUBuffer *gpu,bool dynamic)
{
    if(binding<0||!gpu)
        return(false);

    if(binded_sets.Contains(binding))return(false);

    const VkDescriptorType desc_type=dynamic?VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    vab_list.push_back(gpu->GetDescriptorBufferInfo());
    const int buf_index=(int)vab_list.size()-1;
    wds_list.push_back(WriteDescriptorSet(desc_set,binding,(const VkDescriptorBufferInfo *)nullptr,desc_type));
    wds_buffer_info_indices.push_back(buf_index);
    wds_image_info_indices.push_back(-1);
    SyncWriteDescriptorInfoPointers();

    binded_sets.Add(binding);
    is_dirty=true;
    return(true);
}

bool DescriptorSet::BindTexture(const int binding,Texture *tex)
{
    if(binding<0||!tex)
        return(false);

    if(binded_sets.Contains(binding))return(false);

    DescriptorImageInfo image_info(tex);

    image_list.push_back(image_info);
    const int image_index = (int)image_list.size()-1;
    wds_list.push_back(WriteDescriptorSet(desc_set,binding,(const VkDescriptorImageInfo *)nullptr));
    wds_buffer_info_indices.push_back(-1);
    wds_image_info_indices.push_back(image_index);
    SyncWriteDescriptorInfoPointers();

    binded_sets.Add(binding);
    is_dirty=true;
    return(true);
}

bool DescriptorSet::BindTextureSampler(const int binding,Texture *tex,Sampler *sampler)
{
    if(binding<0||!tex||!sampler)
        return(false);

    if(binded_sets.Contains(binding))return(false);

    DescriptorImageInfo image_info(tex,sampler);

    image_list.push_back(image_info);
    const int image_index = (int)image_list.size()-1;
    wds_list.push_back(WriteDescriptorSet(desc_set,binding,(const VkDescriptorImageInfo *)nullptr));
    wds_buffer_info_indices.push_back(-1);
    wds_image_info_indices.push_back(image_index);
    SyncWriteDescriptorInfoPointers();

    binded_sets.Add(binding);
    is_dirty=true;
    return(true);
}

bool DescriptorSet::BindInputAttachment(const int binding,ImageView *iv)
{
    if(binding<0||!iv)
        return(false);

    if(binded_sets.Contains(binding))return(false);

    DescriptorImageInfo image_info(iv->GetImageView());

    image_list.push_back(image_info);
    const int image_index = (int)image_list.size()-1;
    wds_list.push_back(WriteDescriptorSet(desc_set,binding,(const VkDescriptorImageInfo *)nullptr,VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT));
    wds_buffer_info_indices.push_back(-1);
    wds_image_info_indices.push_back(image_index);
    SyncWriteDescriptorInfoPointers();

    binded_sets.Add(binding);
    is_dirty=true;
    return(true);
}

void DescriptorSet::Update()
{
    if(!is_dirty)return;

    SyncWriteDescriptorInfoPointers();

    if(wds_list.size()>0)
    {
        vkUpdateDescriptorSets(device,(uint32_t)wds_list.size(),wds_list.data(),0,nullptr);
    }

    Clear();
}
}//namespace hgl::graph
