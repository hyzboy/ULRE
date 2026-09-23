#include <hgl/vk/VKGlobalSceneUBOSet.h>
#include <hgl/vk/buffer/IGPUBuffer.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/log/Log.h>
#include <hgl/common/ShaderStageDef.h>

namespace hgl::graph
{

bool GlobalSceneUBOSet::Init(VkDevice device)
{
    device_ = device;

    VulkanDevice *vdev = VulkanDevice::FromDevice(device);
    if (vdev && vdev->GetDevAttr())
        push_fn_ = vdev->GetDevAttr()->cmd_push_descriptor_set;

    if (!push_fn_)
    {
        GLogError(u8"[GlobalSceneUBOSet] cmd_push_descriptor_set not found (pushDescriptor not supported?)");
        return false;
    }

    // ── 描述符集布局（camera=0 / sky=1 / viewport=2 / color_palette=3 / global_addresses=4 / shadow=5）──
    // stageFlags 加 COMPUTE：compute 管线复用全局 layout 时可按需读这些 UBO
    //（如按 viewport 尺寸定 dispatch 维度）；graphics 侧不受影响（stage 声明超集合法）。
    {
        constexpr uint32_t kBindingCount = uint32_t(SceneBinding::RANGE_SIZE);
        VkDescriptorSetLayoutBinding bindings[kBindingCount]{};

        // binding=0 : camera
        bindings[0].binding         = uint32_t(kSceneBindingCamera);
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags      = hgl::graph::kMeshFragment | VK_SHADER_STAGE_COMPUTE_BIT;

        // binding=1 : sky
        bindings[1].binding         = uint32_t(kSceneBindingSky);
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags      = hgl::graph::kMeshFragment | VK_SHADER_STAGE_COMPUTE_BIT;

        // binding=2 : viewport
        bindings[2].binding         = uint32_t(kSceneBindingViewport);
        bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags      = hgl::graph::kMeshFragment | VK_SHADER_STAGE_COMPUTE_BIT;

        // binding=3 : color_palette
        bindings[3].binding         = uint32_t(kSceneBindingColorPalette);
        bindings[3].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[3].descriptorCount = 1;
        bindings[3].stageFlags      = hgl::graph::kMeshFragment | VK_SHADER_STAGE_COMPUTE_BIT;

        // binding=4 : global_addresses
        bindings[4].binding         = uint32_t(kSceneBindingGlobalAddresses);
        bindings[4].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[4].descriptorCount = 1;
        bindings[4].stageFlags      = hgl::graph::kMeshFragment | VK_SHADER_STAGE_COMPUTE_BIT;

        // binding=5 : shadow
        bindings[5].binding         = uint32_t(kSceneBindingShadow);
        bindings[5].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[5].descriptorCount = 1;
        bindings[5].stageFlags      = hgl::graph::kMeshFragment | VK_SHADER_STAGE_COMPUTE_BIT;

        // PARTIALLY_BOUND：允许未写入的 binding（如 palette/sky/shadow）保持为空而不触发校验错误。
        VkDescriptorBindingFlags binding_flags[kBindingCount];
        for (uint32_t i = 0; i < kBindingCount; ++i)
            binding_flags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo flags_ci{};
        flags_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flags_ci.bindingCount  = kBindingCount;
        flags_ci.pBindingFlags = binding_flags;

        VkDescriptorSetLayoutCreateInfo layout_ci{};
        layout_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_ci.pNext        = &flags_ci;
        layout_ci.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR
                               | VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        layout_ci.bindingCount = kBindingCount;
        layout_ci.pBindings    = bindings;

        if (vkCreateDescriptorSetLayout(device_, &layout_ci, nullptr, &layout_) != VK_SUCCESS)
        {
            GLogError(u8"[GlobalSceneUBOSet] Failed to create push descriptor set layout");
            return false;
        }
    }

    GLogInfo(u8"[GlobalSceneUBOSet] Initialized with Push Descriptor (camera=0, sky=1, viewport=2, color_palette=3, global_addresses=4, shadow=5)");
    return true;
}

void GlobalSceneUBOSet::Destroy()
{
    if (device_ == VK_NULL_HANDLE)
        return;

    if (layout_ != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device_, layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
    }

    push_fn_ = nullptr;
    device_ = VK_NULL_HANDLE;

    for (size_t i = 0; i < size_t(SceneBinding::RANGE_SIZE); ++i)
    {
        bound_buffers_info_[i] = {};
        binding_valid_[i] = false;
    }
}

bool GlobalSceneUBOSet::UpdateUBO(uint32_t binding, const IGPUBuffer *gpu)
{
    if (layout_ == VK_NULL_HANDLE || binding >= uint32_t(SceneBinding::RANGE_SIZE))
        return false;

    if (!gpu)
    {
        binding_valid_[binding] = false;
        bound_buffers_info_[binding] = {};
        return true;
    }

    const VkBuffer vk_buf = gpu->GetVkDeviceBuffer();
    if (vk_buf == VK_NULL_HANDLE)
    {
        binding_valid_[binding] = false;
        bound_buffers_info_[binding] = {};
        return false;
    }

    bound_buffers_info_[binding].buffer = vk_buf;
    bound_buffers_info_[binding].offset = 0;
    bound_buffers_info_[binding].range  = gpu->GetSize();
    binding_valid_[binding]             = true;
    return true;
}

void GlobalSceneUBOSet::BindToCmd(VkCommandBuffer cmd, VkPipelineLayout pipeline_layout,
                                  VkPipelineBindPoint bind_point) const
{
    if (layout_ == VK_NULL_HANDLE || !push_fn_ || cmd == VK_NULL_HANDLE)
        return;

    constexpr uint32_t kMaxBindings = uint32_t(SceneBinding::RANGE_SIZE);
    VkWriteDescriptorSet writes[kMaxBindings];
    uint32_t write_count = 0;

    for (uint32_t i = 0; i < kMaxBindings; ++i)
    {
        if (!binding_valid_[i])
            continue;

        VkWriteDescriptorSet &w = writes[write_count++];
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.pNext           = nullptr;
        w.dstSet          = VK_NULL_HANDLE;
        w.dstBinding      = i;
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pImageInfo      = nullptr;
        w.pBufferInfo     = &bound_buffers_info_[i];
        w.pTexelBufferView= nullptr;
    }

    if (write_count > 0)
    {
        push_fn_(cmd,
                 bind_point,
                 pipeline_layout,
                 uint32_t(DescriptorSetType::Scene),
                 write_count,
                 writes);
    }
}

}//namespace hgl::graph
