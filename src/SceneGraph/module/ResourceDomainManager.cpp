#include <hgl/graph/module/ResourceDomainManager.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/vk/VKBuffer.h>

namespace hgl::graph
{
GRAPH_MODULE_CONSTRUCT(ResourceDomainManager)
{
}

uint64_t ResourceDomainManager::MakeKey(const mtl::SSBOType ssbo_type, const uint32_t resource_domain_id) noexcept
{
    return (static_cast<uint64_t>(ssbo_type) << 32) | static_cast<uint64_t>(resource_domain_id);
}

ResourceDomainBinding *ResourceDomainManager::FindMutable(const mtl::SSBOType ssbo_type, const uint32_t resource_domain_id)
{
    auto it = domain_map.find(MakeKey(ssbo_type, resource_domain_id));
    return it == domain_map.end() ? nullptr : &it->second;
}

const ResourceDomainBinding *ResourceDomainManager::Find(const mtl::SSBOType ssbo_type, const uint32_t resource_domain_id) const
{
    auto it = domain_map.find(MakeKey(ssbo_type, resource_domain_id));
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

bool ResourceDomainManager::Touch(const mtl::SSBOType ssbo_type, const uint32_t resource_domain_id)
{
    const uint64_t key = MakeKey(ssbo_type, resource_domain_id);
    auto it = domain_map.find(key);
    if (it != domain_map.end())
        return true;

    ResourceDomainBinding binding{};
    binding.ssbo_type = ssbo_type;
    binding.resource_domain_id = resource_domain_id;
    domain_map.emplace(key, binding);
    return true;
}

bool ResourceDomainManager::RegisterBuffer(const mtl::SSBOType ssbo_type,
                                           const uint32_t resource_domain_id,
                                           DeviceBuffer *buffer,
                                           const uint32_t element_capacity)
{
    if (!buffer)
        return false;

    const uint64_t key = MakeKey(ssbo_type, resource_domain_id);
    auto &binding = domain_map[key];

    if (binding.buffer && binding.buffer != buffer)
    {
        auto *buffer_manager = GetGraphicsContext() ? GetGraphicsContext()->GetBufferManager() : nullptr;
        if (buffer_manager)
            buffer_manager->Release(binding.buffer);
        else
            delete binding.buffer;
    }

    binding.ssbo_type = ssbo_type;
    binding.resource_domain_id = resource_domain_id;
    binding.buffer = buffer;
    binding.element_capacity = element_capacity;
    return true;
}

DeviceBuffer *ResourceDomainManager::EnsureBuffer(const mtl::SSBOType ssbo_type,
                                                  const uint32_t resource_domain_id,
                                                  const AnsiString &name,
                                                  const VkDeviceSize byte_size,
                                                  const uint32_t required_capacity,
                                                  const SharingMode sm)
{
    if (byte_size == 0 || required_capacity == 0)
        return GetBuffer(ssbo_type, resource_domain_id);

    auto *gc = GetGraphicsContext();
    auto *buffer_manager = gc ? gc->GetBufferManager() : nullptr;
    if (!buffer_manager)
        return nullptr;

    const uint64_t key = MakeKey(ssbo_type, resource_domain_id);
    auto &binding = domain_map[key];
    binding.ssbo_type = ssbo_type;
    binding.resource_domain_id = resource_domain_id;

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

bool ResourceDomainManager::ClearDomain(const mtl::SSBOType ssbo_type, const uint32_t resource_domain_id)
{
    auto it = domain_map.find(MakeKey(ssbo_type, resource_domain_id));
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

DeviceBuffer *ResourceDomainManager::GetBuffer(const mtl::SSBOType ssbo_type, const uint32_t resource_domain_id) const
{
    const auto *binding = Find(ssbo_type, resource_domain_id);
    return binding ? binding->buffer : nullptr;
}

const IGPUBuffer *ResourceDomainManager::GetGPUBuffer(const mtl::SSBOType ssbo_type, const uint32_t resource_domain_id) const
{
    const auto *buffer = GetBuffer(ssbo_type, resource_domain_id);
    return buffer ? buffer->GetGPUBuffer() : nullptr;
}

uint32_t ResourceDomainManager::GetElementCapacity(const mtl::SSBOType ssbo_type, const uint32_t resource_domain_id) const
{
    const auto *binding = Find(ssbo_type, resource_domain_id);
    return binding ? binding->element_capacity : 0;
}
} // namespace hgl::graph

