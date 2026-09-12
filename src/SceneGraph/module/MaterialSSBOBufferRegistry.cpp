#include <hgl/graph/module/MaterialSSBOBufferRegistry.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <hgl/log/Log.h>

#include <climits>

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
        material_row_pools[index].Reset();

    material_data_buffers_initialized = false;
}

bool MaterialSSBOBufferRegistry::IsMaterialDataIDActive(
    const mtl::MaterialSSBOType material_type,
    const uint32_t data_id) const
{
    const auto *pool = GetMaterialRowPool(material_type);
    return pool && pool->IsActive(data_id);
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

    auto *pool = GetMaterialRowPool(material_type);
    if (!pool || pool->GetBuffer())
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Duplicate default material buffer: type=%s",
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

    const uint32_t ssbo_id = mtl::MakeRecipeSSBOId(
        GetMaterialSSBOTypeIndex(material_type) + 1u);

    if (!pool->Create(GetDevice(),
                      name,
                      row_stride,
                      DefaultMaterialDataElementCapacity,
                      ssbo_id))
    {
        GLogError(
            "[MaterialSSBOBufferRegistry] Default material buffer creation failed: type=%s",
            mtl::GetMaterialSSBOTypeName(material_type));
        return false;
    }

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
        const auto &pool = material_row_pools[index];
        if (pool.GetSSBOId() != ssbo_id || !pool.GetBuffer())
            continue;

        out_info.material_ssbo_type = static_cast<mtl::MaterialSSBOType>(
            index + static_cast<uint32_t>(mtl::MaterialSSBOType::BEGIN_RANGE));
        out_info.cpu_base = pool.GetCPUBase();
        out_info.gpu_base = pool.GetGPUBase();
        out_info.row_bytes = pool.GetRowBytes();
        out_info.row_capacity = pool.GetRowCapacity();
        out_info.buffer = pool.GetBuffer();
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
