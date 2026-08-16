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
        VkDescriptorPoolSize pool_sizes[1] = {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMax }
        };

        VkDescriptorPoolCreateInfo pool_ci{};
        pool_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_ci.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        pool_ci.maxSets       = 1;
        pool_ci.poolSizeCount = 1;
        pool_ci.pPoolSizes    = pool_sizes;

        if (vkCreateDescriptorPool(device_, &pool_ci, nullptr, &pool_) != VK_SUCCESS)
        {
            GLogError("[BindlessTextureManager] Failed to create descriptor pool");
            return false;
        }
    }

    // ── 描述符集布局 ──────────────────────────────────────────────────
    {
        VkDescriptorSetLayoutBinding bindings[1]{};

        // binding=0 : sampler2DArray[]
        bindings[0].binding         = 0;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount = kMax;
        bindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorBindingFlags flags[1] = {
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
        };

        VkDescriptorSetLayoutBindingFlagsCreateInfo flags_ci{};
        flags_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flags_ci.bindingCount  = 1;
        flags_ci.pBindingFlags = flags;

        VkDescriptorSetLayoutCreateInfo layout_ci{};
        layout_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_ci.pNext        = &flags_ci;
        layout_ci.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layout_ci.bindingCount = 1;
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

    GLogInfo("[BindlessTextureManager] Initialized (max=%u)", kMax);
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

    device_       = VK_NULL_HANDLE;
    next_handle_  = 1;
    handle_cache_.Clear();
}

uint32_t BindlessTextureManager::Register(Texture *tex, Sampler *sampler)
{
    if (!tex || !sampler || set_ == VK_NULL_HANDLE)
        return 0;

    const uint64_t key = MakeCacheKey(tex, sampler);
    const uint32_t *cached = handle_cache_.GetValuePointer(key);
    if (cached)
        return *cached;

    if (next_handle_ > kMax)
    {
        GLogError("[BindlessTextureManager] handle exhausted (max=%u)", kMax);
        return 0;
    }

    const uint32_t handle = next_handle_++;
    handle_cache_.Add(key, handle);

    VkDescriptorImageInfo img_info{};
    img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img_info.imageView   = tex->GetBindlessArrayView();
    img_info.sampler     = static_cast<VkSampler>(*sampler);

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = set_;
    write.dstBinding      = 0;
    write.dstArrayElement = handle - 1;   // 0-based array index
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo      = &img_info;

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

    GLogInfo("[BindlessTextureManager] Register handle=%u", handle);
    return handle;
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
