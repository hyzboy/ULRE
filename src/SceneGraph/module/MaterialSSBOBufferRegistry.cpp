#include <hgl/graph/module/MaterialSSBOBufferRegistry.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/log/Log.h>
#include <hgl/type/Smart.h>
#include <hgl/vk/buffer/DeviceBuffer.h>

#include <climits>
#include <cstring>

namespace hgl::graph
{
GRAPH_MODULE_CONSTRUCT(MaterialSSBOBufferRegistry)
{
}

void MaterialSSBOBufferRegistry::OnGraphicsContextChanged(
    GraphicsContext *graphics_context)
{
    if (!graphics_context || material_data_buffers_initialized)
        return;

    if (!InitializeMaterialDataBuffers())
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Failed to create default material data buffers");
    }
}

void MaterialSSBOBufferRegistry::Release()
{
    for (uint32_t index = 0; index < MaterialSSBOTypeCount; ++index)
    {
        auto &storage = material_buffers[index];
        storage.ids.Clear(true);

        if (storage.buffer)
        {
            auto *gpu_buffer = storage.buffer->GetGPUBuffer();
            if (storage.cpu_base && gpu_buffer)
                gpu_buffer->Unmap();

            SAFE_CLEAR(storage.buffer);
        }

        storage.buffer = nullptr;
        storage.cpu_base = nullptr;
        storage.gpu_base = 0;
        storage.ssbo_id = 0;
        storage.row_bytes = 0;
        storage.row_capacity = 0;
    }

    material_data_buffers_initialized = false;
}

bool MaterialSSBOBufferRegistry::TryGetMaterialBinding(
    const mtl::MaterialSSBOType material_type,
    MaterialSSBOBufferBinding &out_binding) const
{
    const auto *storage = GetMaterialBufferStorage(material_type);
    if (!storage || !storage->buffer || storage->ssbo_id == 0)
        return false;

    out_binding.material_ssbo_type = material_type;
    out_binding.ssbo_id = storage->ssbo_id;
    out_binding.buffer = storage->buffer;
    out_binding.element_capacity = storage->row_capacity;
    out_binding.element_stride = storage->row_bytes;
    return true;
}

DeviceBuffer *MaterialSSBOBufferRegistry::GetMaterialBuffer(
    const mtl::MaterialSSBOType material_type) const
{
    const auto *storage = GetMaterialBufferStorage(material_type);
    return storage ? storage->buffer : nullptr;
}

const IGPUBuffer *MaterialSSBOBufferRegistry::GetMaterialGPUBuffer(
    const mtl::MaterialSSBOType material_type) const
{
    const auto *buffer = GetMaterialBuffer(material_type);
    return buffer ? buffer->GetGPUBuffer() : nullptr;
}

uint32_t MaterialSSBOBufferRegistry::GetMaterialElementCapacity(
    const mtl::MaterialSSBOType material_type) const
{
    const auto *storage = GetMaterialBufferStorage(material_type);
    return storage ? storage->row_capacity : 0;
}

uint32_t MaterialSSBOBufferRegistry::GetMaterialSSBOId(
    const mtl::MaterialSSBOType material_type) const
{
    const auto *storage = GetMaterialBufferStorage(material_type);
    return storage ? storage->ssbo_id : 0;
}

bool MaterialSSBOBufferRegistry::IsMaterialDataIDActive(
    const mtl::MaterialSSBOType material_type,
    const uint32_t data_id) const
{
    const auto *storage = GetMaterialBufferStorage(material_type);
    if (!storage || data_id > static_cast<uint32_t>(INT_MAX))
        return false;

    return storage->ids.IsActive(static_cast<int>(data_id));
}

bool MaterialSSBOBufferRegistry::CreateMaterialDataBuffer(
    const mtl::MaterialSSBOType material_type,
    const AnsiString &name)
{
    if (!mtl::IsMaterialSSBOType(material_type))
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Default material buffer rejected invalid type=%s",
            mtl::GetMaterialSSBOTypeName(material_type));
        return false;
    }

    auto *storage = GetMaterialBufferStorage(material_type);
    if (!storage || storage->buffer)
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Duplicate default material buffer: type=%s",
            mtl::GetMaterialSSBOTypeName(material_type));
        return false;
    }

    VulkanDevice *device = GetDevice();
    if (!device)
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Default material buffer creation failed without device: type=%s",
            mtl::GetMaterialSSBOTypeName(material_type));
        return false;
    }

    const uint32_t row_stride =
        mtl::GetMaterialSSBOTypeStructStride(material_type);
    if (row_stride == 0)
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Default material buffer rejected zero row stride: type=%s",
            mtl::GetMaterialSSBOTypeName(material_type));
        return false;
    }

    AutoDelete<DeviceBuffer> buffer(device->CreateArenaBuffer(
        name,
        VkDeviceSize(row_stride) * DefaultMaterialDataElementCapacity));
    if (!buffer)
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Default material buffer allocation failed: type=%s",
            mtl::GetMaterialSSBOTypeName(material_type));
        return false;
    }

    auto *gpu_buffer = buffer->GetGPUBuffer();
    if (!gpu_buffer)
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Default material buffer has no GPU buffer: type=%s",
            mtl::GetMaterialSSBOTypeName(material_type));
        return false;
    }

    const uint64_t gpu_base = device->GetBufferDeviceAddress(buffer->GetBuffer());
    if (gpu_base == 0)
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Default material buffer has no device address: type=%s",
            mtl::GetMaterialSSBOTypeName(material_type));
        return false;
    }

    const VkDeviceSize buffer_bytes =
        VkDeviceSize(row_stride) * DefaultMaterialDataElementCapacity;
    void *cpu_base = gpu_buffer->Map(0, buffer_bytes);
    if (!cpu_base)
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Default material buffer mapping failed: type=%s",
            mtl::GetMaterialSSBOTypeName(material_type));
        return false;
    }

    memset(cpu_base, 0, static_cast<size_t>(buffer_bytes));
    gpu_buffer->MarkDirty(0, buffer_bytes);

    storage->buffer = buffer.Finish();
    storage->cpu_base = cpu_base;
    storage->gpu_base = gpu_base;
    storage->ssbo_id = mtl::MakeRecipeSSBOId(
        GetMaterialSSBOTypeIndex(material_type) + 1u);
    storage->row_bytes = row_stride;
    storage->row_capacity = DefaultMaterialDataElementCapacity;
    return true;
}

bool MaterialSSBOBufferRegistry::AcquireMaterialDataID(
    const mtl::MaterialSSBOType material_type,
    uint32_t &out_data_id)
{
    out_data_id = MaterialSSBODataAccessor<ssbo::PBRSurfaceRow>::InvalidDataID;

    auto *storage = GetMaterialBufferStorage(material_type);
    if (!storage || !storage->buffer || !storage->cpu_base)
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Material data ID allocation failed without an initialized buffer: type=%s",
            mtl::GetMaterialSSBOTypeName(material_type));
        return false;
    }

    if (storage->row_capacity == 0
     || storage->row_capacity > static_cast<uint32_t>(INT_MAX))
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Material data ID allocation rejected invalid capacity: type=%s capacity=%u",
            mtl::GetMaterialSSBOTypeName(material_type),
            storage->row_capacity);
        return false;
    }

    int data_id = -1;
    if (storage->ids.HasIdleID())
    {
        data_id = storage->ids.GetIdle();
    }
    else
    {
        if (storage->ids.GetHistoryMaxId() >=
            static_cast<int>(storage->row_capacity))
        {
            GLogError(
                "[MaterialSSBOBufferRegistry] Material data ID allocation exceeded capacity: type=%s capacity=%u",
                mtl::GetMaterialSSBOTypeName(material_type),
                storage->row_capacity);
            return false;
        }

        if (storage->ids.CreateActive(&data_id) != 1)
        {
            GLogError(
                "[MaterialSSBOBufferRegistry] Material data ID allocation failed: type=%s",
                mtl::GetMaterialSSBOTypeName(material_type));
            return false;
        }
    }

    if (data_id < 0 || static_cast<uint32_t>(data_id) >= storage->row_capacity)
    {
        if (data_id >= 0 && storage->ids.IsActive(data_id))
            storage->ids.Release(&data_id);

        GLogError(
            "[MaterialSSBOBufferRegistry] Material data ID allocation produced an invalid ID: type=%s id=%d capacity=%u",
            mtl::GetMaterialSSBOTypeName(material_type),
            data_id,
            storage->row_capacity);
        return false;
    }

    out_data_id = static_cast<uint32_t>(data_id);
    return true;
}

bool MaterialSSBOBufferRegistry::ReleaseMaterialDataID(
    const mtl::MaterialSSBOType material_type,
    const uint32_t data_id)
{
    auto *storage = GetMaterialBufferStorage(material_type);
    if (!storage
     || data_id == MaterialSSBODataAccessor<ssbo::PBRSurfaceRow>::InvalidDataID
     || data_id > static_cast<uint32_t>(INT_MAX)
     || !storage->ids.IsActive(static_cast<int>(data_id)))
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Material data ID release rejected inactive ID: type=%s id=%u",
            mtl::GetMaterialSSBOTypeName(material_type),
            data_id);
        return false;
    }

    const int raw_data_id = static_cast<int>(data_id);
    return storage->ids.Release(&raw_data_id) == 1;
}

bool MaterialSSBOBufferRegistry::CommitMaterialData(
    const mtl::MaterialSSBOType material_type,
    const uint32_t data_id)
{
    auto *storage = GetMaterialBufferStorage(material_type);
    if (!storage
     || !storage->buffer
     || !storage->cpu_base
     || data_id >= storage->row_capacity
     || !IsMaterialDataIDActive(material_type, data_id))
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Material data commit rejected invalid row: type=%s id=%u",
            mtl::GetMaterialSSBOTypeName(material_type),
            data_id);
        return false;
    }

    auto *gpu_buffer = storage->buffer->GetGPUBuffer();
    if (!gpu_buffer)
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Material data commit failed without GPU buffer: type=%s id=%u",
            mtl::GetMaterialSSBOTypeName(material_type),
            data_id);
        return false;
    }

    const VkDeviceSize offset = VkDeviceSize(data_id) * storage->row_bytes;
    gpu_buffer->MarkDirty(offset, storage->row_bytes);
    return true;
}

bool MaterialSSBOBufferRegistry::TryGetRowBuffer(
    const uint32_t ssbo_id,
    MaterialRowBufferInfo &out_info) const
{
    if (ssbo_id == 0)
        return false;

    for (uint32_t index = 0; index < MaterialSSBOTypeCount; ++index)
    {
        const auto &storage = material_buffers[index];
        if (storage.ssbo_id != ssbo_id || !storage.buffer)
            continue;

        out_info.material_ssbo_type = static_cast<mtl::MaterialSSBOType>(
            index + static_cast<uint32_t>(mtl::MaterialSSBOType::BEGIN_RANGE));
        out_info.cpu_base = storage.cpu_base;
        out_info.gpu_base = storage.gpu_base;
        out_info.row_bytes = storage.row_bytes;
        out_info.row_capacity = storage.row_capacity;
        out_info.buffer = storage.buffer;
        return true;
    }

    return false;
}

bool MaterialSSBOBufferRegistry::InitializeMaterialDataBuffers()
{
    if (material_data_buffers_initialized)
        return true;

    ENUM_CLASS_FOR(mtl::MaterialSSBOType, uint32_t, type_index)
    {
        const auto material_type =
            static_cast<mtl::MaterialSSBOType>(type_index);
        const AnsiString buffer_name =
            AnsiString("Default:")
            + AnsiString(mtl::GetMaterialSSBOTypeName(material_type))
            + AnsiString("MaterialData");

        if (!CreateMaterialDataBuffer(material_type, buffer_name))
        {
            Release();
            return false;
        }
    }

    material_data_buffers_initialized = true;
    return true;
}
} // namespace hgl::graph
