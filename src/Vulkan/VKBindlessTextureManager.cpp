#include <hgl/vk/VKBindlessTextureManager.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKTexture.h>
#include <hgl/vk/VKSampler.h>
#include <hgl/log/Log.h>
#include <cstring>

namespace hgl::graph
{

bool BindlessTextureManager::InitDescriptorBuffer()
{
    if (!attr_ || !attr_->physical_device)
        return false;

    if (!attr_->physical_device->SupportDescriptorBuffer())
    {
        LogError(u8"[BindlessTextureManager] 硬件或驱动不支持 VK_EXT_descriptor_buffer");
        return false;
    }

    if (!attr_->get_descriptor_set_layout_size ||
        !attr_->get_descriptor_set_layout_binding_offset ||
        !attr_->get_descriptor ||
        !attr_->cmd_bind_descriptor_buffers ||
        !attr_->cmd_set_descriptor_buffer_offsets)
    {
        LogError(u8"[BindlessTextureManager] Descriptor Buffer 扩展函数指针未正确加载");
        return false;
    }

    const auto &props = attr_->physical_device->GetDescriptorBufferProperties();

    // ── 描述符集布局（带 DESCRIPTOR_BUFFER 标志） ───────────────────────
    {
        VkDescriptorSetLayoutBinding bindings[3]{};

        // binding=0 : texture2DArray[]（SAMPLED_IMAGE，非均匀索引）
        bindings[0].binding         = 0;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[0].descriptorCount = kMax;
        bindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

        // binding=1 : sampler[]（SAMPLER，非均匀索引）
        bindings[1].binding         = 1;
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
        bindings[1].descriptorCount = kMaxSampler;
        bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

        // binding=2 : textureCubeArray[]
        bindings[2].binding         = 2;
        bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[2].descriptorCount = kMax;
        bindings[2].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorBindingFlags flags[3] = {
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
        };

        VkDescriptorSetLayoutBindingFlagsCreateInfo flags_ci{};
        flags_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flags_ci.bindingCount  = 3;
        flags_ci.pBindingFlags = flags;

        VkDescriptorSetLayoutCreateInfo layout_ci{};
        layout_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_ci.pNext        = &flags_ci;
        layout_ci.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        layout_ci.bindingCount = 3;
        layout_ci.pBindings    = bindings;

        if (vkCreateDescriptorSetLayout(device_, &layout_ci, nullptr, &layout_) != VK_SUCCESS)
        {
            LogError(u8"[BindlessTextureManager] 创建 Descriptor Buffer 布局失败");
            return false;
        }
    }

    // ── 查询布局大小与 Binding 偏移 ────────────────────────────────────
    attr_->get_descriptor_set_layout_size(device_, layout_, &layout_size_);

    const VkDeviceSize alignment = props.descriptorBufferOffsetAlignment;
    if (alignment > 0)
    {
        layout_size_ = (layout_size_ + alignment - 1) & ~(alignment - 1);
    }

    attr_->get_descriptor_set_layout_binding_offset(device_, layout_, 0, &binding_offset_0_);
    attr_->get_descriptor_set_layout_binding_offset(device_, layout_, 1, &binding_offset_1_);
    attr_->get_descriptor_set_layout_binding_offset(device_, layout_, 2, &binding_offset_2_);
    sampled_image_desc_size_ = props.sampledImageDescriptorSize;
    sampler_desc_size_       = props.samplerDescriptorSize;

    // ── 创建描述符缓冲区 ───────────────────────────────────────────────
    VkBufferCreateInfo buf_ci{};
    buf_ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_ci.size        = layout_size_;
    buf_ci.usage       = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT
                       | VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
                       | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    buf_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &buf_ci, nullptr, &desc_buffer_) != VK_SUCCESS)
    {
        LogError(u8"[BindlessTextureManager] vkCreateBuffer 失败");
        Destroy();
        return false;
    }

    VkMemoryRequirements mem_reqs{};
    vkGetBufferMemoryRequirements(device_, desc_buffer_, &mem_reqs);

    uint32_t mem_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                       | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                       | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    int mem_type = attr_->physical_device->GetMemoryType(mem_reqs.memoryTypeBits, mem_props);
    if (mem_type < 0)
    {
        mem_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        mem_type = attr_->physical_device->GetMemoryType(mem_reqs.memoryTypeBits, mem_props);
    }
    if (mem_type < 0)
    {
        LogError(u8"[BindlessTextureManager] 无法为 Descriptor Buffer 找到合适的 host-visible 内存类型");
        Destroy();
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

    if (vkAllocateMemory(device_, &alloc_info, nullptr, &desc_memory_) != VK_SUCCESS)
    {
        LogError(u8"[BindlessTextureManager] vkAllocateMemory 失败");
        Destroy();
        return false;
    }

    if (vkBindBufferMemory(device_, desc_buffer_, desc_memory_, 0) != VK_SUCCESS)
    {
        LogError(u8"[BindlessTextureManager] vkBindBufferMemory 失败");
        Destroy();
        return false;
    }

    if (vkMapMemory(device_, desc_memory_, 0, layout_size_, 0, reinterpret_cast<void**>(&mapped_ptr_)) != VK_SUCCESS)
    {
        LogError(u8"[BindlessTextureManager] vkMapMemory 失败");
        Destroy();
        return false;
    }

    memset(mapped_ptr_, 0, static_cast<size_t>(layout_size_));

    VkBufferDeviceAddressInfo addr_info{};
    addr_info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addr_info.buffer = desc_buffer_;
    desc_buffer_address_ = vkGetBufferDeviceAddress(device_, &addr_info);

    if (desc_buffer_address_ == 0)
    {
        LogError(u8"[BindlessTextureManager] vkGetBufferDeviceAddress 获取地址失败");
        Destroy();
        return false;
    }

    attr_->use_descriptor_buffer = true;
    use_descriptor_buffer_       = true;

    LogInfo(u8"[BindlessTextureManager] 初始化 Descriptor Buffer 模式成功 (layout_size=%llu, offsets=[%llu, %llu, %llu], sampledImageSize=%zu, samplerSize=%zu, addr=0x%llx)",
            static_cast<unsigned long long>(layout_size_),
            static_cast<unsigned long long>(binding_offset_0_),
            static_cast<unsigned long long>(binding_offset_1_),
            static_cast<unsigned long long>(binding_offset_2_),
            sampled_image_desc_size_,
            sampler_desc_size_,
            static_cast<unsigned long long>(desc_buffer_address_));
    return true;
}

#ifdef HGL_VK_DESCRIPTOR_POOL_FALLBACK
bool BindlessTextureManager::InitDescriptorPool()
{
    // ── 描述符池（UPDATE_AFTER_BIND） ──────────────────────────────────
    VkDescriptorPoolSize pool_sizes[3] = {
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, kMax },        // binding=0 texture2DArray
        { VK_DESCRIPTOR_TYPE_SAMPLER,       kMaxSampler }, // binding=1 sampler
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, kMax },        // binding=2 textureCube
    };

    VkDescriptorPoolCreateInfo pool_ci{};
    pool_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_ci.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pool_ci.maxSets       = 1;
    pool_ci.poolSizeCount = 3;
    pool_ci.pPoolSizes    = pool_sizes;

    if (vkCreateDescriptorPool(device_, &pool_ci, nullptr, &pool_) != VK_SUCCESS)
    {
        LogError(u8"[BindlessTextureManager] Failed to create descriptor pool");
        return false;
    }

    // ── 描述符集布局 ──────────────────────────────────────────────────
    VkDescriptorSetLayoutBinding bindings[3]{};

    // binding=0 : texture2DArray[]（SAMPLED_IMAGE，非均匀索引）
    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[0].descriptorCount = kMax;
    bindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

    // binding=1 : sampler[]（SAMPLER，非均匀索引）
    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindings[1].descriptorCount = kMaxSampler;
    bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

    // binding=2 : textureCubeArray[]
    bindings[2].binding         = 2;
    bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[2].descriptorCount = kMax;
    bindings[2].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorBindingFlags flags[3] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_ci{};
    flags_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flags_ci.bindingCount  = 3;
    flags_ci.pBindingFlags = flags;

    VkDescriptorSetLayoutCreateInfo layout_ci{};
    layout_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_ci.pNext        = &flags_ci;
    layout_ci.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layout_ci.bindingCount = 3;
    layout_ci.pBindings    = bindings;

    if (vkCreateDescriptorSetLayout(device_, &layout_ci, nullptr, &layout_) != VK_SUCCESS)
    {
        LogError(u8"[BindlessTextureManager] Failed to create descriptor set layout");
        return false;
    }

    // ── 描述符集分配 ──────────────────────────────────────────────────
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool     = pool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts        = &layout_;

    if (vkAllocateDescriptorSets(device_, &alloc_info, &set_) != VK_SUCCESS)
    {
        LogError(u8"[BindlessTextureManager] Failed to allocate descriptor set");
        return false;
    }

    use_descriptor_buffer_ = false;

    LogInfo(u8"[BindlessTextureManager] 初始化 DescriptorPool 模式成功 (max_tex=%u, max_sampler=%u)", kMax, kMaxSampler);
    return true;
}
#endif//HGL_VK_DESCRIPTOR_POOL_FALLBACK

bool BindlessTextureManager::Init(VulkanDevice *device)
{
    if (!device)
        return false;
    return Init(device->GetDevice(), device->GetDevAttr());
}

bool BindlessTextureManager::Init(VkDevice device, VulkanDevAttr *attr)
{
    device_ = device;
    attr_   = attr;

    if (!attr_ && device_)
    {
        if (auto *vk_dev = VulkanDevice::FromDevice(device_))
            attr_ = vk_dev->GetDevAttr();
    }

    if (attr_ && attr_->use_descriptor_buffer && attr_->physical_device && attr_->physical_device->SupportDescriptorBuffer())
    {
        if (InitDescriptorBuffer())
            return true;

#ifdef HGL_VK_DESCRIPTOR_POOL_FALLBACK
        LogWarning(u8"[BindlessTextureManager] 初始化 Descriptor Buffer 模式失败，回退到 DescriptorPool 模式");
        Destroy();
        device_ = device;
        attr_   = attr;
#else
        LogError(u8"[BindlessTextureManager] 初始化 Descriptor Buffer 模式失败");
        return false;
#endif//HGL_VK_DESCRIPTOR_POOL_FALLBACK
    }

#ifdef HGL_VK_DESCRIPTOR_POOL_FALLBACK
    return InitDescriptorPool();
#else
    LogError(u8"[BindlessTextureManager] 设备未启用 Descriptor Buffer，无可用回退路径");
    return false;
#endif//HGL_VK_DESCRIPTOR_POOL_FALLBACK
}

void BindlessTextureManager::Destroy()
{
    if (device_ == VK_NULL_HANDLE)
        return;

#ifdef HGL_VK_DESCRIPTOR_POOL_FALLBACK
    if (pool_ != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device_, pool_, nullptr);
        pool_ = VK_NULL_HANDLE;
        set_  = VK_NULL_HANDLE;
    }
#endif//HGL_VK_DESCRIPTOR_POOL_FALLBACK

    if (mapped_ptr_)
    {
        vkUnmapMemory(device_, desc_memory_);
        mapped_ptr_ = nullptr;
    }

    if (desc_buffer_ != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device_, desc_buffer_, nullptr);
        desc_buffer_ = VK_NULL_HANDLE;
    }

    if (desc_memory_ != VK_NULL_HANDLE)
    {
        vkFreeMemory(device_, desc_memory_, nullptr);
        desc_memory_ = VK_NULL_HANDLE;
    }

    desc_buffer_address_ = 0;
    layout_size_         = 0;
    binding_offset_0_    = 0;
    binding_offset_1_    = 0;
    binding_offset_2_    = 0;
    sampled_image_desc_size_ = 0;
    sampler_desc_size_       = 0;

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

    device_                = VK_NULL_HANDLE;
    attr_                  = nullptr;
    use_descriptor_buffer_ = false;
    next_handle_           = 1;
    tex_cache_.Clear();
}

uint32_t BindlessTextureManager::AllocateHandle(Texture *placeholder_tex)
{
    if (!IsValid())
        return 0;

    const uint32_t tex_handle = next_handle_++;
    if (placeholder_tex)
    {
        UpdateTextureHandle(tex_handle, placeholder_tex);
    }
    return tex_handle;
}

bool BindlessTextureManager::UpdateTextureHandle(uint32_t tex_handle, Texture *tex)
{
    if (!tex || !IsValid() || tex_handle == 0 || tex_handle >= next_handle_)
        return false;

    // Cubemap 纹理写 binding=2(textureCube[])，其余写 binding=0(texture2DArray[])
    const VkImageView cube_view = tex->GetBindlessCubeView();

    // 采样描述符只接受"可采样布局"。纹理跟踪布局可能是 PRESENT_SRC_KHR 或附件布局
    // （渲染刚写过），此时按采样前的目标布局注册为 SHADER_READ_ONLY_OPTIMAL。
    const VkImageLayout tex_layout = tex->GetImageLayout();

    if (tex_layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
     && tex_layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
     && tex_layout != VK_IMAGE_LAYOUT_GENERAL)
    {
        LogWarning(u8"[BindlessTextureManager] 纹理 %p 当前布局 %d 不可采样，按 SHADER_READ_ONLY_OPTIMAL 注册",
                   (const void *)tex, static_cast<int>(tex_layout));
    }

    VkDescriptorImageInfo img_info{};
    img_info.imageLayout = (tex_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                         || tex_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                         || tex_layout == VK_IMAGE_LAYOUT_GENERAL)
                         ? tex_layout
                         : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img_info.imageView   = cube_view ? cube_view
                                     : tex->GetBindlessArrayView();

    if (img_info.imageView == VK_NULL_HANDLE)
    {
        LogError(u8"[BindlessTextureManager] Failed to get valid image view for texture %p", (const void *)tex);
        return false;
    }

    tex_cache_.Add(tex, tex_handle);

#ifdef HGL_VK_DESCRIPTOR_POOL_FALLBACK
    if (!use_descriptor_buffer_)
    {
        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = set_;
        write.dstBinding      = cube_view ? 2 : 0;
        write.dstArrayElement = tex_handle - 1;   // 0-based array index
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo      = &img_info;

        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    }
    else
#endif//HGL_VK_DESCRIPTOR_POOL_FALLBACK
    {
        VkDescriptorGetInfoEXT get_info{};
        get_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get_info.type               = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        get_info.data.pSampledImage = &img_info;

        const VkDeviceSize base_offset = cube_view ? binding_offset_2_ : binding_offset_0_;
        const VkDeviceSize item_offset = base_offset + (tex_handle - 1) * sampled_image_desc_size_;

        attr_->get_descriptor(device_, &get_info, sampled_image_desc_size_, mapped_ptr_ + item_offset);
    }

    LogInfo(u8"[BindlessTextureManager] Update handle=%u tex=%p (%s) view=%p",
            tex_handle,
            (const void *)tex,
            cube_view ? "cubearray" : "2darray",
            img_info.imageView);
    return true;
}

uint32_t BindlessTextureManager::RegisterTexture(Texture *tex)
{
    if (!tex || !IsValid())
        return 0;

    const uint32_t *cached = tex_cache_.GetValuePointer(tex);
    if (cached)
        return *cached;

    const uint32_t tex_handle = next_handle_++;
    if (!UpdateTextureHandle(tex_handle, tex))
        return 0;

    return tex_handle;
}

bool BindlessTextureManager::RegisterSamplers(const VkSamplerCreateInfo *infos, uint32_t count)
{
    if (!infos || count == 0)
        return false;
    if (!IsValid())
        return false;
    if (count > kMaxSampler)
    {
        LogError(u8"[BindlessTextureManager] sampler count %u exceeds max %u", count, kMaxSampler);
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
            LogError(u8"[BindlessTextureManager] vkCreateSampler failed idx=%u", i);
            return false;
        }
        samplers_.Add(samp);

#ifdef HGL_VK_DESCRIPTOR_POOL_FALLBACK
        if (!use_descriptor_buffer_)
        {
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
        else
#endif//HGL_VK_DESCRIPTOR_POOL_FALLBACK
        {
            VkDescriptorGetInfoEXT get_info{};
            get_info.sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
            get_info.type           = VK_DESCRIPTOR_TYPE_SAMPLER;
            get_info.data.pSampler  = &samp;

            const VkDeviceSize item_offset = binding_offset_1_ + i * sampler_desc_size_;
            attr_->get_descriptor(device_, &get_info, sampler_desc_size_, mapped_ptr_ + item_offset);
        }
    }

    LogInfo(u8"[BindlessTextureManager] Registered %u samplers", count);
    return true;
}

bool BindlessTextureManager::RebuildSampler(uint32_t index, const VkSamplerCreateInfo &info)
{
    if (!IsValid() || static_cast<int>(index) >= samplers_.GetCount())
        return false;

    // 等待设备空闲，消除运行时重建 Sampler 的同步隐患
    vkDeviceWaitIdle(device_);

    VkSampler new_samp = VK_NULL_HANDLE;
    if (vkCreateSampler(device_, &info, nullptr, &new_samp) != VK_SUCCESS)
    {
        LogError(u8"[BindlessTextureManager] vkCreateSampler failed idx=%u", index);
        return false;
    }

#ifdef HGL_VK_DESCRIPTOR_POOL_FALLBACK
    if (!use_descriptor_buffer_)
    {
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
    }
    else
#endif//HGL_VK_DESCRIPTOR_POOL_FALLBACK
    {
        VkDescriptorGetInfoEXT get_info{};
        get_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        get_info.type          = VK_DESCRIPTOR_TYPE_SAMPLER;
        get_info.data.pSampler = &new_samp;

        const VkDeviceSize item_offset = binding_offset_1_ + index * sampler_desc_size_;
        attr_->get_descriptor(device_, &get_info, sampler_desc_size_, mapped_ptr_ + item_offset);
    }

    // 替换成功后销毁旧句柄
    if (samplers_[static_cast<int>(index)] != VK_NULL_HANDLE)
        vkDestroySampler(device_, samplers_[static_cast<int>(index)], nullptr);
    samplers_[static_cast<int>(index)] = new_samp;

    LogInfo(u8"[BindlessTextureManager] Rebuilt sampler idx=%u", index);
    return true;
}

void BindlessTextureManager::BindOffsetToCmd(VkCommandBuffer cmd,
                                             VkPipelineLayout pipeline_layout,
                                             uint32_t set_index,
                                             uint32_t buffer_index,
                                             VkPipelineBindPoint bind_point) const
{
    if (!IsValid())
        return;

#ifdef HGL_VK_DESCRIPTOR_POOL_FALLBACK
    if (!use_descriptor_buffer_)
    {
        BindToCmd(cmd, pipeline_layout, set_index, bind_point);
        return;
    }
#endif//HGL_VK_DESCRIPTOR_POOL_FALLBACK

    if (!attr_ || !attr_->cmd_set_descriptor_buffer_offsets)
        return;

    const VkDeviceSize buffer_offset = 0;
    attr_->cmd_set_descriptor_buffer_offsets(cmd,
                                            bind_point,
                                            pipeline_layout,
                                            set_index,
                                            1,
                                            &buffer_index,
                                            &buffer_offset);
}

void BindlessTextureManager::BindToCmd(VkCommandBuffer cmd,
                                       VkPipelineLayout pipeline_layout,
                                       uint32_t set_index,
                                       VkPipelineBindPoint bind_point) const
{
    if (!IsValid())
        return;

#ifdef HGL_VK_DESCRIPTOR_POOL_FALLBACK
    if (!use_descriptor_buffer_)
    {
        if (set_ != VK_NULL_HANDLE)
        {
            vkCmdBindDescriptorSets(cmd,
                                    bind_point,
                                    pipeline_layout,
                                    set_index,
                                    1, &set_,
                                    0, nullptr);
        }
        return;
    }
#endif//HGL_VK_DESCRIPTOR_POOL_FALLBACK

    if (!attr_ || !attr_->cmd_bind_descriptor_buffers)
        return;

    VkDescriptorBufferBindingInfoEXT binding_info{};
    binding_info.sType   = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    binding_info.pNext   = nullptr;
    binding_info.address = desc_buffer_address_;
    binding_info.usage   = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT
                         | VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    attr_->cmd_bind_descriptor_buffers(cmd, 1, &binding_info);

    BindOffsetToCmd(cmd, pipeline_layout, set_index, 0, bind_point);
}

}//namespace hgl::graph
