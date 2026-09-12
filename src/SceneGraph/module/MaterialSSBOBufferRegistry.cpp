#include <hgl/graph/module/MaterialSSBOBufferRegistry.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <hgl/log/Log.h>

namespace hgl::graph
{
GRAPH_MODULE_CONSTRUCT(MaterialSSBOBufferRegistry)
{
}

void MaterialSSBOBufferRegistry::Release()
{
    auto *buffer_manager = GetGraphicsContext() ? GetGraphicsContext()->GetBufferManager() : nullptr;

    ResetMaterialDataAccessors();

    for (auto &kv : material_domain_map)
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

    row_buffers.Clear();
    material_domain_map.Clear();
    next_ssbo_id = 1;
}

bool MaterialSSBOBufferRegistry::RegisterMaterialBuffer(const mtl::MaterialSSBOType material_type,
                                                       DeviceBuffer *buffer,
                                                       const uint32_t element_capacity)
{
    if (!mtl::IsMaterialSSBOType(material_type)
     || !buffer
     || element_capacity == 0)
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Register material buffer rejected: type=%s buffer=%p capacity=%u",
            mtl::GetMaterialSSBOTypeName(material_type),
            buffer,
            element_capacity);
        return false;
    }

    const uint32_t key = static_cast<uint32_t>(material_type);
    if (material_domain_map.GetValuePointer(key))
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Material buffer already registered: type=%s",
            mtl::GetMaterialSSBOTypeName(material_type));
        return false;
    }

    MaterialSSBOBufferBinding binding{};
    binding.material_ssbo_type = material_type;
    binding.ssbo_id = AllocateMaterialSSBOId();
    binding.buffer = buffer;
    binding.element_capacity = element_capacity;
    binding.element_stride = mtl::GetMaterialSSBOTypeStructStride(material_type);

    if (binding.element_stride == 0)
        return false;

    const uint64_t gpu_base = GetDevice() ? GetDevice()->GetBufferDeviceAddress(buffer->GetBuffer()) : 0;
    if (gpu_base == 0)
        return false;

    void *cpu_base = nullptr;
    if (!BindMaterialDataAccessor(
            material_type,
            binding.ssbo_id,
            buffer,
            element_capacity,
            cpu_base))
        return false;

    material_domain_map[key] = binding;
    row_buffers[binding.ssbo_id] = MaterialRowBufferInfo{
        material_type,
        cpu_base,
        gpu_base,
        binding.element_stride,
        binding.element_capacity,
        buffer};
    return true;
}

bool MaterialSSBOBufferRegistry::TryGetMaterialBinding(const mtl::MaterialSSBOType material_type,
                                                      MaterialSSBOBufferBinding &out_binding) const
{
    const auto *binding =
        material_domain_map.GetValuePointer(static_cast<uint32_t>(material_type));
    if (!binding)
        return false;

    out_binding = *binding;
    return true;
}

DeviceBuffer *MaterialSSBOBufferRegistry::GetMaterialBuffer(const mtl::MaterialSSBOType material_type) const
{
    MaterialSSBOBufferBinding binding{};
    return TryGetMaterialBinding(material_type, binding) ? binding.buffer : nullptr;
}

const IGPUBuffer *MaterialSSBOBufferRegistry::GetMaterialGPUBuffer(const mtl::MaterialSSBOType material_type) const
{
    const auto *buffer = GetMaterialBuffer(material_type);
    return buffer ? buffer->GetGPUBuffer() : nullptr;
}

uint32_t MaterialSSBOBufferRegistry::GetMaterialElementCapacity(const mtl::MaterialSSBOType material_type) const
{
    MaterialSSBOBufferBinding binding{};
    return TryGetMaterialBinding(material_type, binding) ? binding.element_capacity : 0;
}

uint32_t MaterialSSBOBufferRegistry::GetMaterialSSBOId(const mtl::MaterialSSBOType material_type) const
{
    MaterialSSBOBufferBinding binding{};
    return TryGetMaterialBinding(material_type, binding) ? binding.ssbo_id : 0;
}

bool MaterialSSBOBufferRegistry::IsMaterialDataIDActive(
    const mtl::MaterialSSBOType material_type,
    const uint32_t data_id) const
{
    switch (material_type)
    {
    case mtl::MaterialSSBOType::PBRSurface:
        return pbr_surface_rows.IsActiveID(data_id);
    case mtl::MaterialSSBOType::EmissiveSurface:
        return emissive_surface_rows.IsActiveID(data_id);
    case mtl::MaterialSSBOType::TransmissionSurface:
        return transmission_surface_rows.IsActiveID(data_id);
    default:
        return false;
    }
}

bool MaterialSSBOBufferRegistry::EnsureMaterialDataSSBO(const mtl::MaterialSSBOType material_type,
                                                       const AnsiString &name,
                                                       const uint32_t element_count,
                                                       const SharingMode sm)
{
    if (!mtl::IsMaterialSSBOType(material_type) || element_count == 0)
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Ensure material buffer rejected: type=%s minimum_capacity=%u",
            mtl::GetMaterialSSBOTypeName(material_type),
            element_count);
        return false;
    }

    const uint32_t key = static_cast<uint32_t>(material_type);
    if (const auto *existing =
            material_domain_map.GetValuePointer(key);
        existing && existing->buffer)
    {
        if (existing->element_capacity >= element_count)
            return true;

        GLogError(
            "[MaterialSSBOBufferRegistry] Material data buffer cannot grow: type=%s capacity=%u requested=%u",
            mtl::GetMaterialSSBOTypeName(material_type),
            existing->element_capacity,
            element_count);
        return false;
    }

    VulkanDevice *device = GetDevice();
    if (!device)
        return false;

    const uint32_t row_stride = mtl::GetMaterialSSBOTypeStructStride(material_type);
    if (row_stride == 0)
        return false;

    const uint32_t element_capacity =
        element_count > DefaultMaterialDataElementCapacity
            ? element_count
            : DefaultMaterialDataElementCapacity;
    DeviceBuffer *buf = device->CreateArenaBuffer(
        name,
        VkDeviceSize(row_stride) * element_capacity);
    if (!buf)
        return false;

    const uint64_t gpu_base = device->GetBufferDeviceAddress(buf->GetBuffer());
    if (gpu_base == 0)
    {
        delete buf;
        return false;
    }

    MaterialSSBOBufferBinding binding{};
    binding.material_ssbo_type = material_type;
    binding.ssbo_id = AllocateMaterialSSBOId();
    binding.buffer = buf;
    binding.element_capacity = element_capacity;
    binding.element_stride = row_stride;

    void *active_cpu_base = nullptr;
    if (!BindMaterialDataAccessor(
            material_type,
            binding.ssbo_id,
            buf,
            element_capacity,
            active_cpu_base))
    {
        delete buf;
        return false;
    }

    memset(active_cpu_base, 0, static_cast<size_t>(row_stride) * element_capacity);

    material_domain_map[key] = binding;
    row_buffers[binding.ssbo_id] = MaterialRowBufferInfo{
        material_type,
        active_cpu_base,
        gpu_base,
        row_stride,
        element_capacity,
        buf};
    return true;
}

bool MaterialSSBOBufferRegistry::BindMaterialDataAccessor(
    const mtl::MaterialSSBOType material_type,
    const uint32_t ssbo_id,
    DeviceBuffer *buffer,
    const uint32_t element_count,
    void *&out_cpu_base)
{
    out_cpu_base = nullptr;
    if (!mtl::IsMaterialSSBOType(material_type)
     || !buffer
     || element_count == 0
     || ssbo_id == 0)
        return false;

    switch (material_type)
    {
    case mtl::MaterialSSBOType::PBRSurface:
        if (!pbr_surface_rows.Bind(buffer, element_count))
            return false;
        pbr_surface_rows.GetArrayView().ssbo_id = ssbo_id;
        pbr_surface_rows.GetArrayView().ssbo_type =
            mtl::SSBOType::UserDefined;
        out_cpu_base = pbr_surface_rows.GetArrayView().GetData();
        return out_cpu_base != nullptr;
    case mtl::MaterialSSBOType::EmissiveSurface:
        if (!emissive_surface_rows.Bind(buffer, element_count))
            return false;
        emissive_surface_rows.GetArrayView().ssbo_id = ssbo_id;
        emissive_surface_rows.GetArrayView().ssbo_type =
            mtl::SSBOType::UserDefined;
        out_cpu_base = emissive_surface_rows.GetArrayView().GetData();
        return out_cpu_base != nullptr;
    case mtl::MaterialSSBOType::TransmissionSurface:
        if (!transmission_surface_rows.Bind(buffer, element_count))
            return false;
        transmission_surface_rows.GetArrayView().ssbo_id = ssbo_id;
        transmission_surface_rows.GetArrayView().ssbo_type =
            mtl::SSBOType::UserDefined;
        out_cpu_base = transmission_surface_rows.GetArrayView().GetData();
        return out_cpu_base != nullptr;
    default:
        return false;
    }
}

void MaterialSSBOBufferRegistry::ResetMaterialDataAccessors()
{
    pbr_surface_rows.Reset();
    pbr_surface_rows.GetArrayView().ssbo_id = 0;
    emissive_surface_rows.Reset();
    emissive_surface_rows.GetArrayView().ssbo_id = 0;
    transmission_surface_rows.Reset();
    transmission_surface_rows.GetArrayView().ssbo_id = 0;
}

bool MaterialSSBOBufferRegistry::EnsureMaterialDataSSBOs()
{
    return EnsureMaterialDataSSBO(mtl::MaterialSSBOType::PBRSurface,
                                  AnsiString("Default:PBRSurfaceMaterialData"),
                                  DefaultMaterialDataElementCapacity)
        && EnsureMaterialDataSSBO(mtl::MaterialSSBOType::EmissiveSurface,
                                  AnsiString("Default:EmissiveSurfaceMaterialData"),
                                  DefaultMaterialDataElementCapacity)
        && EnsureMaterialDataSSBO(mtl::MaterialSSBOType::TransmissionSurface,
                                  AnsiString("Default:TransmissionSurfaceMaterialData"),
                                  DefaultMaterialDataElementCapacity);
}

} // namespace hgl::graph
