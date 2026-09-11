#include <hgl/graph/module/SSBOBufferRegistry.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/vk/VKBuffer.h>
#include <hgl/log/Log.h>
#include <hgl/type/Smart.h>

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
                  source_tag ? source_tag : "SSBOBufferRegistry",
                  mtl::GetSSBOTypeName(address.ssbo_type),
                  address.ssbo_id,
                  expected_version,
                  expected_stride,
                  element_stride);
        return false;
    }
}

GRAPH_MODULE_CONSTRUCT(SSBOBufferRegistry)
{
}

uint32_t SSBOBufferRegistry::AllocateSSBOId()
{
    return mtl::MakeRecipeSSBOId(next_ssbo_id++);
}

uint64_t SSBOBufferRegistry::MakeKey(const mtl::SSBOAddress &address) noexcept
{
    return (static_cast<uint64_t>(address.ssbo_type) << 32) | static_cast<uint64_t>(address.ssbo_id);
}

SSBOBufferBinding *SSBOBufferRegistry::FindMutable(const mtl::SSBOAddress &address)
{
    auto it = domain_map.find(MakeKey(address));
    return it == domain_map.end() ? nullptr : &it->second;
}

const SSBOBufferBinding *SSBOBufferRegistry::Find(const mtl::SSBOAddress &address) const
{
    auto it = domain_map.find(MakeKey(address));
    return it == domain_map.end() ? nullptr : &it->second;
}

MaterialTextureReferencePool *
    SSBOBufferRegistry::FindMaterialTextureReferencePool(
        const mtl::MaterialDefinition &definition,
        const mtl::MaterialTextureReferenceLayout &layout)
{
    for (int i = 0;
         i < material_texture_reference_pools.GetCount();
         ++i)
    {
        MaterialTextureReferencePool *pool =
            material_texture_reference_pools[i];
        if (pool && pool->Matches(definition, layout))
            return pool;
    }
    return nullptr;
}

MaterialTextureReferencePool *
    SSBOBufferRegistry::FindMaterialTextureReferencePool(
        const MaterialTextureConfigurationAllocation &allocation)
{
    if (!allocation.IsValid())
        return nullptr;

    for (int i = 0;
         i < material_texture_reference_pools.GetCount();
         ++i)
    {
        MaterialTextureReferencePool *pool =
            material_texture_reference_pools[i];
        if (pool && pool->GetPoolKey() == allocation.pool_key)
            return pool;
    }
    return nullptr;
}

uint64_t SSBOBufferRegistry::GetNullRowAddress()
{
    if (null_row_address)
        return null_row_address;

    auto *device = GetDevice();
    if (!device)
        return 0;

    null_row_buffer = device->CreateArenaBuffer("NullMaterialRow", 64);
    if (!null_row_buffer)
        return 0;

    if (auto *m = null_row_buffer->GetGPUBuffer()->Map(0, 64))
        memset(m, 0, 64);

    null_row_address = device->GetBufferDeviceAddress(null_row_buffer->GetBuffer());
    return null_row_address;
}

void SSBOBufferRegistry::Release()
{
    for (int i = 0;
         i < material_texture_reference_pools.GetCount();
         ++i)
    {
        if (MaterialTextureReferencePool *pool =
                material_texture_reference_pools[i])
            pool->Release();
    }
    material_texture_reference_pools.Clear();

    if (null_row_buffer)
    {
        delete null_row_buffer;
        null_row_buffer = nullptr;
    }
    null_row_address = 0;

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

    // 行缓冲（AllocateArrayAccessor 建的 arena 缓冲）归注册表所有：
    // 视图只管在窗口上写，不负责释放（B-2：视图不拥有数据源）
    for (auto &kv : row_segments)
        delete kv.second.buffer;

    row_segments.clear();
}

bool SSBOBufferRegistry::AcquireMaterialTextureConfiguration(
    const mtl::MaterialDefinition &definition,
    const mtl::MaterialTextureReferenceLayout &layout,
    MaterialTextureConfigurationAllocation &out_allocation)
{
    out_allocation = {};
    if (!layout.HasReferences())
    {
        GLogError(
            "[MaterialTextureReferencePool] allocation rejected: definition=%s has no texture references",
            definition.definition_id.c_str());
        return false;
    }

    MaterialTextureReferencePool *pool =
        FindMaterialTextureReferencePool(definition, layout);
    if (!pool)
    {
        pool = material_texture_reference_pools.Create();
        if (!pool)
            return false;

        if (!pool->Initialize(GetDevice(), definition, layout))
        {
            GLogError(
                "[MaterialTextureReferencePool] initialization failed: definition=%s layout_hash=%llu references=%u row_stride=%u capacity=%u",
                definition.definition_id.c_str(),
                static_cast<unsigned long long>(layout.layout_hash),
                layout.reference_count,
                layout.row_stride,
                layout.max_configuration_count);
            material_texture_reference_pools.DeleteAt(
                material_texture_reference_pools.GetCount() - 1);
            return false;
        }
    }

    return pool->Acquire(out_allocation);
}

bool SSBOBufferRegistry::WriteMaterialTextureConfiguration(
    const MaterialTextureConfigurationAllocation &allocation,
    const mtl::MaterialTextureReference *references,
    const uint32_t reference_count)
{
    MaterialTextureReferencePool *pool =
        FindMaterialTextureReferencePool(allocation);
    if (!pool || !pool->Write(allocation, references, reference_count))
    {
        GLogError(
            "[MaterialTextureReferencePool] write rejected: pool=%llu row=%u generation=%llu references=%u",
            static_cast<unsigned long long>(allocation.pool_key),
            allocation.row_index,
            static_cast<unsigned long long>(
                allocation.allocation_generation),
            reference_count);
        return false;
    }
    return true;
}

bool SSBOBufferRegistry::IsMaterialTextureConfigurationValid(
    const MaterialTextureConfigurationAllocation &allocation)
{
    MaterialTextureReferencePool *pool =
        FindMaterialTextureReferencePool(allocation);
    return pool && pool->IsValidAllocation(allocation);
}

uint64_t SSBOBufferRegistry::GetMaterialTextureConfigurationZeroRowAddress(
    const mtl::MaterialDefinition &definition,
    const mtl::MaterialTextureReferenceLayout &layout)
{
    MaterialTextureReferencePool *pool =
        FindMaterialTextureReferencePool(definition, layout);
    return pool ? pool->GetZeroRowAddress() : 0;
}

bool SSBOBufferRegistry::RetireMaterialTextureConfiguration(
    const MaterialTextureConfigurationAllocation &allocation,
    const uint64_t retire_epoch)
{
    MaterialTextureReferencePool *pool =
        FindMaterialTextureReferencePool(allocation);
    if (!pool || !pool->Retire(allocation, retire_epoch))
    {
        GLogError(
            "[MaterialTextureReferencePool] retirement rejected: pool=%llu row=%u generation=%llu retire_epoch=%llu",
            static_cast<unsigned long long>(allocation.pool_key),
            allocation.row_index,
            static_cast<unsigned long long>(
                allocation.allocation_generation),
            static_cast<unsigned long long>(retire_epoch));
        return false;
    }
    return true;
}

void SSBOBufferRegistry::CollectRetiredMaterialTextureConfigurations(
    const uint64_t completed_epoch)
{
    for (int i = 0;
         i < material_texture_reference_pools.GetCount();
         ++i)
    {
        if (MaterialTextureReferencePool *pool =
                material_texture_reference_pools[i])
            pool->CollectRetired(completed_epoch);
    }
}

bool SSBOBufferRegistry::Touch(const mtl::SSBOAddress &address)
{
    const uint64_t key = MakeKey(address);
    auto it = domain_map.find(key);
    if (it != domain_map.end())
        return true;

    SSBOBufferBinding binding{};
    binding.ssbo_type = address.ssbo_type;
    binding.ssbo_id = address.ssbo_id;
    domain_map.emplace(key, binding);
    return true;
}

bool SSBOBufferRegistry::RegisterBuffer(const mtl::SSBOAddress &address,
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

DeviceBuffer *SSBOBufferRegistry::EnsureBuffer(const mtl::SSBOAddress &address,
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

    DeviceBuffer *old_buffer = binding.buffer;
    DeviceBuffer *new_buffer = buffer_manager->CreateSSBO(name, byte_size, sm);

    if (!new_buffer)
    {
        binding.buffer = nullptr;
        binding.element_capacity = 0;
        binding.element_stride = 0;
    }
    else
    {
        // Copy-on-grow: preserve previously written rows when a domain buffer
        // must be resized. Releasing the old buffer without copying would wipe
        // rows written earlier in the same pass (EnsureBuffer growth used to
        // zero previously-uploaded data for every domain user).
        if (old_buffer)
        {
            IGPUBuffer *old_gpu = old_buffer->GetGPUBuffer();
            IGPUBuffer *new_gpu = new_buffer->GetGPUBuffer();

            if (old_gpu && new_gpu)
            {
                const VkDeviceSize old_bytes = old_buffer->GetSize();
                void *old_ptr = old_gpu->Map(0, old_bytes);
                if (old_ptr)
                {
                    new_gpu->Write(old_ptr, 0, old_bytes);
                    old_gpu->Unmap();
                }
            }
        }

        binding.buffer = new_buffer;
        binding.element_capacity = required_capacity;
        binding.element_stride = requested_stride;
    }

    if (old_buffer)
        buffer_manager->Release(old_buffer);

    return binding.buffer;
}

bool SSBOBufferRegistry::ClearDomain(const mtl::SSBOAddress &address)
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

bool SSBOBufferRegistry::HasBinding(const mtl::SSBOAddress &address) const
{
    return Find(address) != nullptr;
}

bool SSBOBufferRegistry::TryGetBinding(const mtl::SSBOAddress &address, SSBOBufferBinding &out_binding) const
{
    const auto *binding = Find(address);
    if (!binding)
        return false;

    out_binding = *binding;
    return true;
}

DeviceBuffer *SSBOBufferRegistry::GetBuffer(const mtl::SSBOAddress &address) const
{
    const auto *binding = Find(address);
    return binding ? binding->buffer : nullptr;
}

const IGPUBuffer *SSBOBufferRegistry::GetGPUBuffer(const mtl::SSBOAddress &address) const
{
    const auto *buffer = GetBuffer(address);
    return buffer ? buffer->GetGPUBuffer() : nullptr;
}

uint32_t SSBOBufferRegistry::GetElementCapacity(const mtl::SSBOAddress &address) const
{
    const auto *binding = Find(address);
    return binding ? binding->element_capacity : 0;
}
} // namespace hgl::graph
