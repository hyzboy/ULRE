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

int DescriptorSet::FindWriteDescriptorIndex(const int binding,const VkDescriptorType desc_type) const
{
    const int write_count = write_descriptor_sets.GetCount();
    for (int i = 0; i < write_count; ++i)
    {
        const VkWriteDescriptorSet &wds = write_descriptor_sets[i];
        if (static_cast<int>(wds.dstBinding) == binding
         && wds.descriptorType == desc_type)
            return i;
    }

    return -1;
}

void DescriptorSet::SyncWriteDescriptorInfoPointers()
{
    VkDescriptorBufferInfo *buffer_base = buffer_info_list.GetData();
    VkDescriptorImageInfo *image_base = image_info_list.GetData();

    const int write_count = write_descriptor_sets.GetCount();
    const int buffer_count = buffer_info_list.GetCount();
    const int image_count = image_info_list.GetCount();

    for (int i = 0; i < write_count; ++i)
    {
        VkWriteDescriptorSet &wds = write_descriptor_sets[i];

        const int buf_index = (i < write_buffer_info_indices.GetCount()) ? write_buffer_info_indices[i] : -1;
        const int img_index = (i < write_image_info_indices.GetCount()) ? write_image_info_indices[i] : -1;

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

bool DescriptorSet::UpdateOrAppendBufferBinding(const int binding,const VkDescriptorType desc_type,const VkDescriptorBufferInfo &new_info)
{
    const int write_index = FindWriteDescriptorIndex(binding, desc_type);
    if (write_index >= 0)
    {
        const int existing_buf_index = (write_index < write_buffer_info_indices.GetCount()) ? write_buffer_info_indices[write_index] : -1;
        if (existing_buf_index >= 0 && existing_buf_index < buffer_info_list.GetCount())
        {
            if (buffer_info_list[existing_buf_index] == new_info)
            {
                touched_bindings.Add(binding);
                return true;
            }

            buffer_info_list[existing_buf_index] = new_info;
        }
        else
        {
            const int new_buf_index = buffer_info_list.Add(new_info);
            if (write_index >= write_buffer_info_indices.GetCount())
                write_buffer_info_indices.Add(new_buf_index);
            else
                write_buffer_info_indices[write_index] = new_buf_index;
        }

        if (write_index < write_image_info_indices.GetCount())
            write_image_info_indices[write_index] = -1;

        SyncWriteDescriptorInfoPointers();
        touched_bindings.Add(binding);
        is_dirty = true;
        return true;
    }

    const int new_buf_index = buffer_info_list.Add(new_info);
    write_descriptor_sets.Add(WriteDescriptorSet(desc_set,binding,(const VkDescriptorBufferInfo *)nullptr,desc_type));
    write_buffer_info_indices.Add(new_buf_index);
    write_image_info_indices.Add(-1);
    SyncWriteDescriptorInfoPointers();

    touched_bindings.Add(binding);
    is_dirty = true;
    return true;
}

bool DescriptorSet::UpdateOrAppendImageBinding(const int binding,const VkDescriptorType desc_type,const VkDescriptorImageInfo &new_info)
{
    const int write_index = FindWriteDescriptorIndex(binding, desc_type);
    if (write_index >= 0)
    {
        const int existing_image_index = (write_index < write_image_info_indices.GetCount()) ? write_image_info_indices[write_index] : -1;
        if (existing_image_index >= 0 && existing_image_index < image_info_list.GetCount())
        {
            if (image_info_list[existing_image_index] == new_info)
            {
                touched_bindings.Add(binding);
                return true;
            }

            image_info_list[existing_image_index] = new_info;
        }
        else
        {
            const int new_image_index = image_info_list.Add(new_info);
            if (write_index >= write_image_info_indices.GetCount())
                write_image_info_indices.Add(new_image_index);
            else
                write_image_info_indices[write_index] = new_image_index;
        }

        if (write_index < write_buffer_info_indices.GetCount())
            write_buffer_info_indices[write_index] = -1;

        SyncWriteDescriptorInfoPointers();
        touched_bindings.Add(binding);
        is_dirty = true;
        return true;
    }

    const int new_image_index = image_info_list.Add(new_info);
    write_descriptor_sets.Add(WriteDescriptorSet(desc_set,binding,(const VkDescriptorImageInfo *)nullptr,desc_type));
    write_buffer_info_indices.Add(-1);
    write_image_info_indices.Add(new_image_index);
    SyncWriteDescriptorInfoPointers();

    touched_bindings.Add(binding);
    is_dirty = true;
    return true;
}

void DescriptorSet::Clear()
{
    buffer_info_list.Clear();
    image_info_list.Clear();
    write_descriptor_sets.Clear();
    write_buffer_info_indices.Clear();
    write_image_info_indices.Clear();
    touched_bindings.Clear();
    is_dirty=false;
}

bool DescriptorSet::BindUBO(const int binding,const VkBufferOwner *buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic)
{
    if(binding<0||!buf)
        return(false);

    const VkDescriptorType desc_type=dynamic?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    return UpdateOrAppendBufferBinding(binding, desc_type, DescriptorBufferInfo(buf,offset,range));
}

bool DescriptorSet::BindSSBO(const int binding,const VkBufferOwner *buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic)
{
    if(binding<0||!buf)
        return(false);

    const VkDescriptorType desc_type=dynamic?VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    return UpdateOrAppendBufferBinding(binding, desc_type, DescriptorBufferInfo(buf,offset,range));
}

bool DescriptorSet::BindSSBO(const int binding,const VkBuffer buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic)
{
    if(binding<0||!buf)
        return(false);

    const VkDescriptorType desc_type=dynamic?VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const VkDescriptorBufferInfo info{buf, offset, range};
    return UpdateOrAppendBufferBinding(binding, desc_type, info);
}

bool DescriptorSet::BindUBO(const int binding,const IGPUBuffer *gpu,bool dynamic)
{
    if(binding<0||!gpu)
        return(false);

    const VkDescriptorType desc_type=dynamic?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    return UpdateOrAppendBufferBinding(binding, desc_type, gpu->GetDescriptorBufferInfo());
}

bool DescriptorSet::BindSSBO(const int binding,const IGPUBuffer *gpu,bool dynamic)
{
    if(binding<0||!gpu)
        return(false);

    const VkDescriptorType desc_type=dynamic?VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    return UpdateOrAppendBufferBinding(binding, desc_type, gpu->GetDescriptorBufferInfo());
}

bool DescriptorSet::BindTexture(const int binding,Texture *tex)
{
    if(binding<0||!tex)
        return(false);

    return UpdateOrAppendImageBinding(binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, DescriptorImageInfo(tex));
}

bool DescriptorSet::BindTextureSampler(const int binding,Texture *tex,Sampler *sampler)
{
    if(binding<0||!tex||!sampler)
        return(false);

    return UpdateOrAppendImageBinding(binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DescriptorImageInfo(tex,sampler));
}

bool DescriptorSet::BindInputAttachment(const int binding,ImageView *iv)
{
    if(binding<0||!iv)
        return(false);

    return UpdateOrAppendImageBinding(binding, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, DescriptorImageInfo(iv->GetImageView()));
}

void DescriptorSet::Update()
{
    fprintf(stderr, "[DIAG] DescriptorSet::Update this=%p dirty=%d wds=%d touched=%d\n",
            (void*)this, is_dirty ? 1 : 0, write_descriptor_sets.GetCount(), touched_bindings.GetCount());
    for (int i = 0; i < write_descriptor_sets.GetCount(); ++i)
        fprintf(stderr, "[DIAG]   wds[%d] binding=%u type=%u buf_idx=%d pBufferInfo=%p\n",
                i, write_descriptor_sets[i].dstBinding,
                (uint)write_descriptor_sets[i].descriptorType,
                i < write_buffer_info_indices.GetCount() ? write_buffer_info_indices[i] : -1,
                (void*)write_descriptor_sets[i].pBufferInfo);

    if(!is_dirty)
    {
        touched_bindings.Clear();
        return;
    }

    SyncWriteDescriptorInfoPointers();

    if(write_descriptor_sets.GetCount()>0)
    {
        //LogInfo(u8"[VKDescriptorSet] Update wds_count=%d image_count=%d buffer_count=%d desc_set=%p",
        //    write_descriptor_sets.GetCount(),
        //    image_info_list.GetCount(),
        //    buffer_info_list.GetCount(),
        //    (void*)desc_set);

        //for(int i=0;i<write_descriptor_sets.GetCount();++i)
        //{
        //    const auto &wds = write_descriptor_sets[i];
        //        LogInfo(u8"  [VKDescriptorSet] WDS[%d] binding=%u type=%u pImageInfo=%p pBufferInfo=%p",
        //            i,
        //            wds.dstBinding,
        //            (uint)wds.descriptorType,
        //            (void*)wds.pImageInfo,
        //            (void*)wds.pBufferInfo);
        //}
        vkUpdateDescriptorSets(device,write_descriptor_sets.GetCount(),write_descriptor_sets.GetData(),0,nullptr);
    }

    touched_bindings.Clear();
    is_dirty=false;
}
}//namespace hgl::graph
