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
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              kMax2D + kMax2DArray }
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
        VkDescriptorSetLayoutBinding bindings[2]{};

        // binding=0 : sampler2D[]
        bindings[0].binding         = 0;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount = kMax2D;
        bindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        // binding=1 : sampler2DArray[]
        bindings[1].binding         = 1;
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].descriptorCount = kMax2DArray;
        bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorBindingFlags flags[2] = {
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
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

    GLogInfo("[BindlessTextureManager] Initialized (max2D=%u, max2DArray=%u)", kMax2D, kMax2DArray);
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

    device_ = VK_NULL_HANDLE;
    next_handle_2d_ = next_handle_2darray_ = 1;
    handle_cache_2d_.clear();
    handle_cache_2darray_.clear();
}

uint32_t BindlessTextureManager::Register2D(Texture *tex, Sampler *sampler)
{
    if (!tex || !sampler || set_ == VK_NULL_HANDLE)
        return 0;

    const uint64_t key = MakeCacheKey(tex, sampler);
    auto it = handle_cache_2d_.find(key);
    if (it != handle_cache_2d_.end())
        return it->second;

    if (next_handle_2d_ > kMax2D)
    {
        GLogError("[BindlessTextureManager] 2D handle exhausted (max=%u)", kMax2D);
        return 0;
    }

    const uint32_t handle = next_handle_2d_++;
    handle_cache_2d_[key] = handle;

    VkDescriptorImageInfo img_info{};
    img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img_info.imageView   = tex->GetVulkanImageView();
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

    GLogInfo("[BindlessTextureManager] Register2D handle=%u", handle);
    return handle;
}

uint32_t BindlessTextureManager::Register2DArray(Texture *tex, Sampler *sampler)
{
    if (!tex || !sampler || set_ == VK_NULL_HANDLE)
        return 0;

    const uint64_t key = MakeCacheKey(tex, sampler);
    auto it = handle_cache_2darray_.find(key);
    if (it != handle_cache_2darray_.end())
        return it->second;

    if (next_handle_2darray_ > kMax2DArray)
    {
        GLogError("[BindlessTextureManager] 2DArray handle exhausted (max=%u)", kMax2DArray);
        return 0;
    }

    const uint32_t handle = next_handle_2darray_++;
    handle_cache_2darray_[key] = handle;

    VkDescriptorImageInfo img_info{};
    img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img_info.imageView   = tex->GetVulkanImageView();
    img_info.sampler     = static_cast<VkSampler>(*sampler);

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = set_;
    write.dstBinding      = 1;
    write.dstArrayElement = handle - 1;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo      = &img_info;

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

    GLogInfo("[BindlessTextureManager] Register2DArray handle=%u", handle);
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
