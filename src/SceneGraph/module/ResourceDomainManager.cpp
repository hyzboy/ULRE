#include <hgl/graph/module/ResourceDomainManager.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/vk/VKBuffer.h>
#include <hgl/log/Log.h>

namespace hgl::graph
{
namespace
{
    bool ValidateStructStrideForDomain(const mtl::SSBOAddress &address,
                                       const uint32_t element_stride,
                                       const char *source_tag)
    {
        const uint32_t expected_version = mtl::GetSSBOTypeStructVersion(address.ssbo_type);
        const uint32_t expected_stride = mtl::GetSSBOTypeStructStride(address.ssbo_type);

        if (expected_version == 0 || expected_stride == 0)
            return true;

        if (element_stride == expected_stride)
            return true;

        GLogError("[R11] %s rejected SSBO domain binding: type=%s ssbo_id=%u version=%u expected_stride=%u actual_stride=%u",
                  source_tag ? source_tag : "ResourceDomainManager",
                  mtl::GetSSBOTypeName(address.ssbo_type),
                  address.ssbo_id,
                  expected_version,
                  expected_stride,
                  element_stride);
        return false;
    }
}

GRAPH_MODULE_CONSTRUCT(ResourceDomainManager)
{
}

uint32_t ResourceDomainManager::AllocateSSBOId()
{
    return mtl::MakeRecipeSSBOId(next_ssbo_id++);
}

uint64_t ResourceDomainManager::MakeKey(const mtl::SSBOAddress &address) noexcept
{
    return (static_cast<uint64_t>(address.ssbo_type) << 32) | static_cast<uint64_t>(address.ssbo_id);
}

ResourceDomainBinding *ResourceDomainManager::FindMutable(const mtl::SSBOAddress &address)
{
    auto it = domain_map.find(MakeKey(address));
    return it == domain_map.end() ? nullptr : &it->second;
}

const ResourceDomainBinding *ResourceDomainManager::Find(const mtl::SSBOAddress &address) const
{
    auto it = domain_map.find(MakeKey(address));
    return it == domain_map.end() ? nullptr : &it->second;
}

void ResourceDomainManager::Release()
{
    auto *buffer_manager = GetGraphicsContext() ? GetGraphicsContext()->GetBufferManager() : nullptr;

    for (auto &kv : domain_map)
    {
        auto &binding = kv.second;
        if (!binding.buffer)
            continue;

        if (buffer_manager)
            buffer_manager->Release(binding.buffer);
        else
            delete binding.buffer;

        binding.buffer = nullptr;
        binding.element_capacity = 0;
        binding.element_stride = 0;
    }

    domain_map.clear();
}

bool ResourceDomainManager::Touch(const mtl::SSBOAddress &address)
{
    const uint64_t key = MakeKey(address);
    auto it = domain_map.find(key);
    if (it != domain_map.end())
        return true;

    ResourceDomainBinding binding{};
    binding.ssbo_type = address.ssbo_type;
    binding.ssbo_id = address.ssbo_id;
    domain_map.emplace(key, binding);
    return true;
}

bool ResourceDomainManager::RegisterBuffer(const mtl::SSBOAddress &address,
                                           DeviceBuffer *buffer,
                                           const uint32_t element_capacity)
{
    if (!buffer)
        return false;

    uint32_t element_stride = 0;
    if (element_capacity > 0)
    {
        const VkDeviceSize bytes = buffer->GetSize();
        if (bytes == 0 || (bytes % element_capacity) != 0)
        {
            GLogError("[R11] RegisterBuffer rejected SSBO domain binding: type=%s ssbo_id=%u buffer_bytes=%llu element_capacity=%u",
                      mtl::GetSSBOTypeName(address.ssbo_type),
                      address.ssbo_id,
                      static_cast<unsigned long long>(bytes),
                      element_capacity);
            return false;
        }

        element_stride = static_cast<uint32_t>(bytes / element_capacity);
    }

    if (!ValidateStructStrideForDomain(address, element_stride, "RegisterBuffer"))
        return false;

    const uint64_t key = MakeKey(address);
    auto &binding = domain_map[key];

    if (binding.buffer && binding.buffer != buffer)
    {
        auto *buffer_manager = GetGraphicsContext() ? GetGraphicsContext()->GetBufferManager() : nullptr;
        if (buffer_manager)
            buffer_manager->Release(binding.buffer);
        else
            delete binding.buffer;
    }

    binding.ssbo_type = address.ssbo_type;
    binding.ssbo_id = address.ssbo_id;
    binding.buffer = buffer;
    binding.element_capacity = element_capacity;
    binding.element_stride = element_stride;
    return true;
}

DeviceBuffer *ResourceDomainManager::EnsureBuffer(const mtl::SSBOAddress &address,
                                                  const AnsiString &name,
                                                  const VkDeviceSize byte_size,
                                                  const uint32_t required_capacity,
                                                  const SharingMode sm)
{
    if (byte_size == 0 || required_capacity == 0)
        return GetBuffer(address);

    auto *gc = GetGraphicsContext();
    auto *buffer_manager = gc ? gc->GetBufferManager() : nullptr;
    if (!buffer_manager)
        return nullptr;

    const uint64_t key = MakeKey(address);
    auto &binding = domain_map[key];
    binding.ssbo_type = address.ssbo_type;
    binding.ssbo_id = address.ssbo_id;

    if ((byte_size % required_capacity) != 0)
    {
        GLogError("[R11] EnsureBuffer rejected SSBO domain request: type=%s ssbo_id=%u byte_size=%llu required_capacity=%u",
                  mtl::GetSSBOTypeName(address.ssbo_type),
                  address.ssbo_id,
                  static_cast<unsigned long long>(byte_size),
                  required_capacity);
        return nullptr;
    }

    const uint32_t requested_stride = static_cast<uint32_t>(byte_size / required_capacity);
    if (!ValidateStructStrideForDomain(address, requested_stride, "EnsureBuffer"))
        return nullptr;

    if (binding.buffer
     && binding.element_capacity >= required_capacity
     && (binding.element_stride == 0 || binding.element_stride == requested_stride))
        return binding.buffer;

    if (binding.buffer)
    {
        buffer_manager->Release(binding.buffer);
        binding.buffer = nullptr;
        binding.element_capacity = 0;
        binding.element_stride = 0;
    }

    binding.buffer = buffer_manager->CreateSSBO(name, byte_size, sm);
    if (binding.buffer)
    {
        binding.element_capacity = required_capacity;
        binding.element_stride = requested_stride;
    }

    return binding.buffer;
}

bool ResourceDomainManager::ClearDomain(const mtl::SSBOAddress &address)
{
    auto it = domain_map.find(MakeKey(address));
    if (it == domain_map.end())
        return false;

    auto &binding = it->second;
    if (binding.buffer)
    {
        auto *buffer_manager = GetGraphicsContext() ? GetGraphicsContext()->GetBufferManager() : nullptr;
        if (buffer_manager)
            buffer_manager->Release(binding.buffer);
        else
            delete binding.buffer;
    }

    binding.element_capacity = 0;
    binding.element_stride = 0;

    domain_map.erase(it);
    return true;
}

bool ResourceDomainManager::HasBinding(const mtl::SSBOAddress &address) const
{
    return Find(address) != nullptr;
}

bool ResourceDomainManager::TryGetBinding(const mtl::SSBOAddress &address, ResourceDomainBinding &out_binding) const
{
    const auto *binding = Find(address);
    if (!binding)
        return false;

    out_binding = *binding;
    return true;
}

DeviceBuffer *ResourceDomainManager::GetBuffer(const mtl::SSBOAddress &address) const
{
    const auto *binding = Find(address);
    return binding ? binding->buffer : nullptr;
}

const IGPUBuffer *ResourceDomainManager::GetGPUBuffer(const mtl::SSBOAddress &address) const
{
    const auto *buffer = GetBuffer(address);
    return buffer ? buffer->GetGPUBuffer() : nullptr;
}

uint32_t ResourceDomainManager::GetElementCapacity(const mtl::SSBOAddress &address) const
{
    const auto *binding = Find(address);
    return binding ? binding->element_capacity : 0;
}
} // namespace hgl::graph
