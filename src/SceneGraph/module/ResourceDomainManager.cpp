#include <hgl/graph/module/ResourceDomainManager.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/vk/VKBuffer.h>

namespace hgl::graph
{
GRAPH_MODULE_CONSTRUCT(ResourceDomainManager)
{
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

    if (binding.buffer && binding.element_capacity >= required_capacity)
        return binding.buffer;

    if (binding.buffer)
    {
        buffer_manager->Release(binding.buffer);
        binding.buffer = nullptr;
        binding.element_capacity = 0;
    }

    binding.buffer = buffer_manager->CreateSSBO(name, byte_size, sm);
    if (binding.buffer)
        binding.element_capacity = required_capacity;

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
