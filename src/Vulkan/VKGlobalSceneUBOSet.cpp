#include <hgl/vk/VKGlobalSceneUBOSet.h>
#include <hgl/vk/IGPUBuffer.h>
#include <hgl/log/Log.h>
#include <hgl/common/ShaderStageDef.h>

namespace hgl::graph
{

bool GlobalSceneUBOSet::Init(VkDevice device)
{
    device_ = device;

    // ── 描述符池 ─────────────────────────────────────────────────────
    {
        VkDescriptorPoolSize pool_sizes[1] = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4 }
        };

        VkDescriptorPoolCreateInfo pool_ci{};
        pool_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_ci.maxSets       = 1;
        pool_ci.poolSizeCount = 1;
        pool_ci.pPoolSizes    = pool_sizes;

        if (vkCreateDescriptorPool(device_, &pool_ci, nullptr, &pool_) != VK_SUCCESS)
        {
            GLogError(u8"[GlobalSceneUBOSet] Failed to create descriptor pool");
            return false;
        }
    }

    // ── 描述符集布局（camera=0 / sky=1 / viewport=2 / color_palette=3）────────────────
    {
        VkDescriptorSetLayoutBinding bindings[4]{};

        // binding=0 : camera
        bindings[0].binding         = uint32_t(kSceneBindingCamera);
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags      = hgl::graph::kMeshFragment;

        // binding=1 : sky
        bindings[1].binding         = uint32_t(kSceneBindingSky);
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags      = hgl::graph::kMeshFragment;

        // binding=2 : viewport
        bindings[2].binding         = uint32_t(kSceneBindingViewport);
        bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags      = hgl::graph::kMeshFragment;

        // binding=3 : color_palette
        bindings[3].binding         = uint32_t(kSceneBindingColorPalette);
        bindings[3].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[3].descriptorCount = 1;
        bindings[3].stageFlags      = hgl::graph::kMeshFragment;

        // PARTIALLY_BOUND：允许未写入的 binding（如 palette/sky）保持为空而不触发校验错误。
        VkDescriptorBindingFlags binding_flags[4] = {
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        };

        VkDescriptorSetLayoutBindingFlagsCreateInfo flags_ci{};
        flags_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flags_ci.bindingCount  = 4;
        flags_ci.pBindingFlags = binding_flags;

        VkDescriptorSetLayoutCreateInfo layout_ci{};
        layout_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_ci.pNext        = &flags_ci;
        layout_ci.bindingCount = 4;
        layout_ci.pBindings    = bindings;

        if (vkCreateDescriptorSetLayout(device_, &layout_ci, nullptr, &layout_) != VK_SUCCESS)
        {
            GLogError(u8"[GlobalSceneUBOSet] Failed to create descriptor set layout");
            return false;
        }
    }

    // ── 描述符集分配 ─────────────────────────────────────────────────
    {
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool     = pool_;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts        = &layout_;

        if (vkAllocateDescriptorSets(device_, &alloc_info, &set_) != VK_SUCCESS)
        {
            GLogError(u8"[GlobalSceneUBOSet] Failed to allocate descriptor set");
            return false;
        }
    }

    GLogInfo(u8"[GlobalSceneUBOSet] Initialized (camera=0, sky=1, viewport=2, color_palette=3)");
    return true;
}

void GlobalSceneUBOSet::Destroy()
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
}

bool GlobalSceneUBOSet::UpdateUBO(uint32_t binding, const IGPUBuffer *gpu)
{
    if (set_ == VK_NULL_HANDLE || !gpu || binding >= 4)
        return false;

    const VkBuffer vk_buf = gpu->GetVkDeviceBuffer();
    if (vk_buf == VK_NULL_HANDLE)
        return false;

    // 同一 buffer 无需重复写入
    if (bound_buffers_[binding] == vk_buf)
        return true;

    VkDescriptorBufferInfo buf_info{};
    buf_info.buffer = vk_buf;
    buf_info.offset = 0;
    buf_info.range  = gpu->GetSize();

    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = set_;
    write.dstBinding      = binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo     = &buf_info;

    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

    bound_buffers_[binding] = vk_buf;
    return true;
}

void GlobalSceneUBOSet::BindToCmd(VkCommandBuffer cmd, VkPipelineLayout pipeline_layout) const
{
    if (set_ == VK_NULL_HANDLE)
        return;

    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout,
                            uint32_t(DescriptorSetType::Scene),
                            1, &set_,
                            0, nullptr);
}

}//namespace hgl::graph
