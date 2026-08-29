#include <hgl/vk/VKBindlessTextureManager.h>
#include <hgl/vk/VKTexture.h>
#include <hgl/vk/VKSampler.h>
#include <hgl/log/Log.h>
#include <cstring>

namespace hgl::graph
{

bool BindlessTextureManager::Init(VkDevice device)
{
    device_ = device;

    // ── 描述符池（UPDATE_AFTER_BIND） ──────────────────────────────────
    {
        VkDescriptorPoolSize pool_sizes[2] = {
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, kMax },
            { VK_DESCRIPTOR_TYPE_SAMPLER,       kMaxSampler }
        };

        VkDescriptorPoolCreateInfo pool_ci{};
        pool_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_ci.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        pool_ci.maxSets       = 1;
        pool_ci.poolSizeCount = 2;
        pool_ci.pPoolSizes    = pool_sizes;

        if (vkCreateDescriptorPool(device_, &pool_ci, nullptr, &pool_) != VK_SUCCESS)
        {
            GLogError("[BindlessTextureManager] Failed to create descriptor pool");
            return false;
        }
    }

    // ── 描述符集布局 ──────────────────────────────────────────────────
    {
        VkDescriptorSetLayoutBinding bindings[2]{};

        // binding=0 : texture2DArray[]（SAMPLED_IMAGE，非均匀索引）
        bindings[0].binding         = 0;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[0].descriptorCount = kMax;
        bindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        // binding=1 : sampler[]（SAMPLER，非均匀索引，惰性小池）
        bindings[1].binding         = 1;
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
        bindings[1].descriptorCount = kMaxSampler;
        bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorBindingFlags flags[2] = {
            // binding=0：纹理支持帧内 update-after-bind
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            // binding=1：采样器池仅 PARTIALLY_BOUND（注册须在集合绑定前）
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
        };

        VkDescriptorSetLayoutBindingFlagsCreateInfo flags_ci{};
        flags_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flags_ci.bindingCount  = 2;
        flags_ci.pBindingFlags = flags;

        VkDescriptorSetLayoutCreateInfo layout_ci{};
        layout_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_ci.pNext        = &flags_ci;
        layout_ci.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layout_ci.bindingCount = 2;
        layout_ci.pBindings    = bindings;

        if (vkCreateDescriptorSetLayout(device_, &layout_ci, nullptr, &layout_) != VK_SUCCESS)
        {
            GLogError("[BindlessTextureManager] Failed to create descriptor set layout");
            return false;
        }
    }

    // ── 描述符集分配 ──────────────────────────────────────────────────
    {
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool     = pool_;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts        = &layout_;

        if (vkAllocateDescriptorSets(device_, &alloc_info, &set_) != VK_SUCCESS)
        {
            GLogError("[BindlessTextureManager] Failed to allocate descriptor set");
            return false;
        }
    }

    GLogInfo("[BindlessTextureManager] Initialized (max_tex=%u max_sampler=%u)", kMax, kMaxSampler);
    return true;
}

void BindlessTextureManager::Destroy()
{
    if (device_ == VK_NULL_HANDLE)
        return;

    // pool 释放时自动释放 set
    if (pool_ != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device_, pool_, nullptr);
        pool_ = VK_NULL_HANDLE;
        set_  = VK_NULL_HANDLE;
    }

    if (layout_ != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device_, layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
    }

    for (int i = 0; i < samplers_.GetCount(); ++i)
    {
        if (samplers_[i] != VK_NULL_HANDLE)
            vkDestroySampler(device_, samplers_[i], nullptr);
    }
    samplers_.Clear();

    device_      = VK_NULL_HANDLE;
    next_handle_ = 1;
    tex_cache_.Clear();
}

uint32_t BindlessTextureManager::RegisterTexture(Texture *tex)
{
    if (!tex || set_ == VK_NULL_HANDLE)
        return 0;

    const uint32_t *cached = tex_cache_.GetValuePointer(tex);
    if (cached)
        return *cached;

    if (next_handle_ > kMax)
    {
        GLogError("[BindlessTextureManager] texture handle exhausted (max=%u)", kMax);
        return 0;
    }

    const uint32_t tex_handle = next_handle_++;
    tex_cache_.Add(tex, tex_handle);

    VkDescriptorImageInfo img_info{};
    img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img_info.imageView   = tex->GetBindlessArrayView();

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = set_;
    write.dstBinding      = 0;
    write.dstArrayElement = tex_handle - 1;   // 0-based array index
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo      = &img_info;

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

    GLogInfo("[BindlessTextureManager] Register texture handle=%u", tex_handle);
    return tex_handle;
}

bool BindlessTextureManager::RegisterSamplers(const VkSamplerCreateInfo *infos, uint32_t count)
{
    if (!infos || count == 0)
        return false;
    if (set_ == VK_NULL_HANDLE)
        return false;
    if (count > kMaxSampler)
    {
        GLogError("[BindlessTextureManager] sampler count %u exceeds max %u", count, kMaxSampler);
        return false;
    }

    // 销毁旧句柄（若已注册）
    for (int i = 0; i < samplers_.GetCount(); ++i)
    {
        if (samplers_[i] != VK_NULL_HANDLE)
            vkDestroySampler(device_, samplers_[i], nullptr);
    }
    samplers_.Clear();

    for (uint32_t i = 0; i < count; ++i)
    {
        VkSampler samp = VK_NULL_HANDLE;
        if (vkCreateSampler(device_, &infos[i], nullptr, &samp) != VK_SUCCESS)
        {
            GLogError("[BindlessTextureManager] vkCreateSampler failed idx=%u", i);
            return false;
        }
        samplers_.Add(samp);

        VkDescriptorImageInfo samp_info{};
        samp_info.sampler = samp;

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = set_;
        write.dstBinding      = 1;
        write.dstArrayElement = i;             // index = 预设索引
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
        write.pImageInfo      = &samp_info;

        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    }

    GLogInfo("[BindlessTextureManager] Registered %u samplers", count);
    return true;
}

bool BindlessTextureManager::RebuildSampler(uint32_t index, const VkSamplerCreateInfo &info)
{
    if (set_ == VK_NULL_HANDLE || static_cast<int>(index) >= samplers_.GetCount())
        return false;

    VkSampler new_samp = VK_NULL_HANDLE;
    if (vkCreateSampler(device_, &info, nullptr, &new_samp) != VK_SUCCESS)
    {
        GLogError("[BindlessTextureManager] vkCreateSampler failed idx=%u", index);
        return false;
    }

    VkDescriptorImageInfo samp_info{};
    samp_info.sampler = new_samp;

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = set_;
    write.dstBinding      = 1;
    write.dstArrayElement = index;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
    write.pImageInfo      = &samp_info;

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

    // 替换成功后销毁旧句柄
    if (samplers_[static_cast<int>(index)] != VK_NULL_HANDLE)
        vkDestroySampler(device_, samplers_[static_cast<int>(index)], nullptr);
    samplers_[static_cast<int>(index)] = new_samp;

    GLogInfo("[BindlessTextureManager] Rebuilt sampler idx=%u", index);
    return true;
}

void BindlessTextureManager::BindToCmd(VkCommandBuffer cmd,
                                        VkPipelineLayout pipeline_layout,
                                        uint32_t set_index) const
{
    if (set_ == VK_NULL_HANDLE)
        return;

    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout,
                            set_index,
                            1, &set_,
                            0, nullptr);
}

}//namespace hgl::graph
