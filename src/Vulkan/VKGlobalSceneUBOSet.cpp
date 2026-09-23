#include <hgl/vk/VKGlobalSceneUBOSet.h>
#include <hgl/vk/buffer/IGPUBuffer.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKPhysicalDevice.h>
#include <hgl/log/Log.h>
#include <hgl/common/ShaderStageDef.h>

namespace hgl::graph
{

bool GlobalSceneUBOSet::Init(VkDevice device)
{
    device_ = device;

    VulkanDevice *vdev = VulkanDevice::FromDevice(device);
    if (vdev && vdev->GetDevAttr())
    {
        attr_    = vdev->GetDevAttr();
        push_fn_ = attr_->cmd_push_descriptor_set;
    }

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
        VkDescriptorSetLayoutCreateInfo layout_ci{};
        layout_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_ci.pNext        = nullptr;
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

    if (attr_ && attr_->physical_device && attr_->physical_device->SupportDescriptorBuffer())
    {
        const auto &props = attr_->physical_device->GetDescriptorBufferProperties();
        needs_push_buffer_ = !props.bufferlessPushDescriptors;
        if (needs_push_buffer_)
        {
            VkBufferCreateInfo buf_ci{};
            buf_ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buf_ci.size        = 65536; // 64KB push buffer
            buf_ci.usage       = VK_BUFFER_USAGE_PUSH_DESCRIPTORS_DESCRIPTOR_BUFFER_BIT_EXT
                               | VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
                               | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(device_, &buf_ci, nullptr, &push_desc_buffer_) != VK_SUCCESS)
            {
                GLogError(u8"[GlobalSceneUBOSet] Failed to create push descriptor buffer");
                return false;
            }

            VkMemoryRequirements mem_reqs{};
            vkGetBufferMemoryRequirements(device_, push_desc_buffer_, &mem_reqs);

            uint32_t mem_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                               | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                               | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            int mem_type = attr_->physical_device->GetMemoryType(mem_reqs.memoryTypeBits, mem_props);
            if (mem_type < 0)
            {
                mem_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                mem_type = attr_->physical_device->GetMemoryType(mem_reqs.memoryTypeBits, mem_props);
            }
            if (mem_type < 0)
            {
                mem_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                mem_type = attr_->physical_device->GetMemoryType(mem_reqs.memoryTypeBits, mem_props);
            }
            if (mem_type < 0)
            {
                GLogError(u8"[GlobalSceneUBOSet] Failed to find suitable memory type for push descriptor buffer");
                return false;
            }

            VkMemoryAllocateFlagsInfo flags_info{};
            flags_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
            flags_info.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

            VkMemoryAllocateInfo alloc_info{};
            alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.pNext           = &flags_info;
            alloc_info.allocationSize  = mem_reqs.size;
            alloc_info.memoryTypeIndex = static_cast<uint32_t>(mem_type);

            if (vkAllocateMemory(device_, &alloc_info, nullptr, &push_desc_memory_) != VK_SUCCESS)
            {
                GLogError(u8"[GlobalSceneUBOSet] Failed to allocate memory for push descriptor buffer");
                return false;
            }

            if (vkBindBufferMemory(device_, push_desc_buffer_, push_desc_memory_, 0) != VK_SUCCESS)
            {
                GLogError(u8"[GlobalSceneUBOSet] Failed to bind memory for push descriptor buffer");
                return false;
            }

            VkBufferDeviceAddressInfo bda_info{};
            bda_info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bda_info.buffer = push_desc_buffer_;
            push_desc_address_ = vkGetBufferDeviceAddress(device_, &bda_info);

            GLogInfo(u8"[GlobalSceneUBOSet] Push descriptor buffer created (size=64KB, addr=0x%llx)",
                     static_cast<unsigned long long>(push_desc_address_));
        }
    }

    GLogInfo(u8"[GlobalSceneUBOSet] Initialized with Push Descriptor (camera=0, sky=1, viewport=2, color_palette=3, global_addresses=4, shadow=5)");
    return true;
}

void GlobalSceneUBOSet::Destroy()
{
    if (device_ == VK_NULL_HANDLE)
        return;

    if (push_desc_buffer_ != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device_, push_desc_buffer_, nullptr);
        push_desc_buffer_ = VK_NULL_HANDLE;
    }
    if (push_desc_memory_ != VK_NULL_HANDLE)
    {
        vkFreeMemory(device_, push_desc_memory_, nullptr);
        push_desc_memory_ = VK_NULL_HANDLE;
    }
    push_desc_address_ = 0;
    needs_push_buffer_ = false;
    attr_ = nullptr;

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
