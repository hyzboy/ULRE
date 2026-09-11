#include <hgl/graph/module/MaterialTextureReferencePool.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <hgl/log/Log.h>
#include <hgl/type/Smart.h>
#include <cstring>

namespace hgl::graph
{
MaterialTextureReferencePool::~MaterialTextureReferencePool()
{
    Release();
}

bool MaterialTextureReferencePool::Matches(
    const mtl::MaterialDefinition &definition,
    const mtl::MaterialTextureReferenceLayout &layout) const noexcept
{
    return pool_key != 0
        && pool_key == MakePoolKey(definition, layout)
        && definition_id == definition.definition_id.c_str()
        && layout_hash == layout.layout_hash
        && reference_count == layout.reference_count
        && row_stride == layout.row_stride
        && max_configuration_count == layout.max_configuration_count;
}

bool MaterialTextureReferencePool::Initialize(
    VulkanDevice *device,
    const mtl::MaterialDefinition &definition,
    const mtl::MaterialTextureReferenceLayout &layout)
{
    if (buffer || !device || !layout.HasReferences()
     || layout.row_stride == 0
     || layout.max_configuration_count == 0
     || layout.max_configuration_count
        >= static_cast<uint32_t>(hgl::HGL_S32_MAX))
        return false;

    const uint64_t key = MakePoolKey(definition, layout);
    if (key == 0)
        return false;

    const uint64_t physical_row_count =
        static_cast<uint64_t>(layout.max_configuration_count) + 1u;
    if (static_cast<uint64_t>(layout.row_stride)
        > ~uint64_t(0) / physical_row_count)
        return false;

    const uint64_t byte_count =
        static_cast<uint64_t>(layout.row_stride) * physical_row_count;
    if (byte_count == 0
     || byte_count > static_cast<uint64_t>(~size_t(0)))
        return false;

    const AnsiString buffer_name =
        AnsiString("MaterialTextureReferences:")
        + AnsiString(definition.definition_id.c_str());
    AutoDelete<DeviceBuffer> new_buffer(
        device->CreateArenaBuffer(buffer_name, static_cast<VkDeviceSize>(byte_count)));
    if (!new_buffer)
        return false;

    IGPUBuffer *gpu_buffer = new_buffer->GetGPUBuffer();
    void *mapped_data = gpu_buffer
        ? gpu_buffer->Map(0, static_cast<VkDeviceSize>(byte_count))
        : nullptr;
    const uint64_t address =
        device->GetBufferDeviceAddressAligned16(new_buffer->GetBuffer());
    if (!mapped_data || address == 0)
    {
        if (mapped_data)
            gpu_buffer->Unmap();
        return false;
    }

    ValueArray<uint32_t> new_free_rows;
    ValueArray<uint64_t> new_row_generations;
    if (!new_free_rows.Resize(
            static_cast<int>(layout.max_configuration_count)))
    {
        gpu_buffer->Unmap();
        return false;
    }
    if (!new_row_generations.Resize(
            static_cast<int>(physical_row_count)))
    {
        gpu_buffer->Unmap();
        return false;
    }

    memset(mapped_data, 0, static_cast<size_t>(byte_count));
    for (uint32_t i = 0; i < layout.max_configuration_count; ++i)
        new_free_rows[static_cast<int>(i)] =
            layout.max_configuration_count - i;

    definition_id = definition.definition_id.c_str();
    pool_key = key;
    layout_hash = layout.layout_hash;
    reference_count = layout.reference_count;
    row_stride = layout.row_stride;
    max_configuration_count = layout.max_configuration_count;
    live_configuration_count = 0;
    next_allocation_generation = 1;
    buffer = new_buffer.Finish();
    cpu_base = mapped_data;
    gpu_base = address;
    free_rows = new_free_rows;
    row_generations = new_row_generations;
    retirements.Clear();

    GLogInfo(
        "[MaterialTextureReferencePool] created definition=%s references=%u row_stride=%u capacity=%u bytes=%llu",
        definition_id.c_str(),
        reference_count,
        row_stride,
        max_configuration_count,
        static_cast<unsigned long long>(byte_count));
    return true;
}

void MaterialTextureReferencePool::Release()
{
    if (buffer)
    {
        if (cpu_base)
        {
            if (IGPUBuffer *gpu_buffer = buffer->GetGPUBuffer())
                gpu_buffer->Unmap();
        }
        SAFE_CLEAR(buffer)
    }

    definition_id = "";
    pool_key = 0;
    layout_hash = 0;
    reference_count = 0;
    row_stride = 0;
    max_configuration_count = 0;
    live_configuration_count = 0;
    next_allocation_generation = 1;
    cpu_base = nullptr;
    gpu_base = 0;
    free_rows.Free();
    row_generations.Free();
    retirements.Free();
}

bool MaterialTextureReferencePool::IsOwnedAllocation(
    const MaterialTextureConfigurationAllocation &allocation) const noexcept
{
    if (!allocation.IsValid()
     || allocation.pool_key != pool_key
     || allocation.row_index > max_configuration_count
     || allocation.reference_count != reference_count
     || allocation.row_stride != row_stride
     || allocation.allocation_generation
        != row_generations[static_cast<int>(allocation.row_index)]
     || !cpu_base
     || gpu_base == 0)
        return false;

    const uint8_t *expected_cpu =
        static_cast<const uint8_t *>(cpu_base)
        + static_cast<size_t>(allocation.row_index) * row_stride;
    const uint64_t expected_gpu =
        gpu_base + static_cast<uint64_t>(allocation.row_index) * row_stride;
    return allocation.cpu_row == expected_cpu
        && allocation.gpu_row == expected_gpu;
}

bool MaterialTextureReferencePool::IsRetired(
    const uint32_t row_index) const noexcept
{
    for (int i = 0; i < retirements.GetCount(); ++i)
    {
        if (retirements[i].row_index == row_index)
            return true;
    }
    return false;
}

bool MaterialTextureReferencePool::ReleaseRow(
    const uint32_t row_index,
    const uint64_t allocation_generation)
{
    if (row_index == 0
     || row_index > max_configuration_count
     || !cpu_base
     || live_configuration_count == 0
     || allocation_generation == 0
     || row_generations[static_cast<int>(row_index)]
        != allocation_generation
     || free_rows.Contains(row_index))
        return false;

    uint8_t *row =
        static_cast<uint8_t *>(cpu_base)
        + static_cast<size_t>(row_index) * row_stride;
    memset(row, 0, row_stride);
    row_generations[static_cast<int>(row_index)] = 0;
    free_rows.Add(row_index);
    --live_configuration_count;
    return true;
}

bool MaterialTextureReferencePool::Acquire(
    MaterialTextureConfigurationAllocation &out_allocation)
{
    out_allocation = {};
    if (!buffer || !cpu_base || gpu_base == 0 || free_rows.IsEmpty())
    {
        GLogError(
            "[MaterialTextureReferencePool] exhausted definition=%s live=%u capacity=%u",
            definition_id.c_str(),
            live_configuration_count,
            max_configuration_count);
        return false;
    }

    const int free_index = free_rows.GetCount() - 1;
    const uint32_t row_index = free_rows[free_index];
    if (row_index == 0 || !free_rows.Delete(free_index))
        return false;

    uint8_t *row =
        static_cast<uint8_t *>(cpu_base)
        + static_cast<size_t>(row_index) * row_stride;
    memset(row, 0, row_stride);

    uint64_t allocation_generation = next_allocation_generation++;
    if (allocation_generation == 0)
        allocation_generation = next_allocation_generation++;
    row_generations[static_cast<int>(row_index)] = allocation_generation;

    ++live_configuration_count;
    out_allocation.pool_key = pool_key;
    out_allocation.row_index = row_index;
    out_allocation.reference_count = reference_count;
    out_allocation.row_stride = row_stride;
    out_allocation.allocation_generation = allocation_generation;
    out_allocation.cpu_row = row;
    out_allocation.gpu_row =
        gpu_base + static_cast<uint64_t>(row_index) * row_stride;
    return true;
}

bool MaterialTextureReferencePool::Write(
    const MaterialTextureConfigurationAllocation &allocation,
    const mtl::MaterialTextureReference *references,
    const uint32_t in_reference_count)
{
    if (!IsOwnedAllocation(allocation)
     || IsRetired(allocation.row_index)
     || !references
     || in_reference_count != reference_count)
        return false;

    memset(allocation.cpu_row, 0, row_stride);
    memcpy(
        allocation.cpu_row,
        references,
        static_cast<size_t>(reference_count)
            * sizeof(mtl::MaterialTextureReference));
    return true;
}

bool MaterialTextureReferencePool::IsValidAllocation(
    const MaterialTextureConfigurationAllocation &allocation)
    const noexcept
{
    return IsOwnedAllocation(allocation)
        && !IsRetired(allocation.row_index);
}

bool MaterialTextureReferencePool::Retire(
    const MaterialTextureConfigurationAllocation &allocation,
    const uint64_t retire_epoch)
{
    if (!IsOwnedAllocation(allocation)
     || IsRetired(allocation.row_index))
        return false;

    return retirements.Add(
        MaterialTextureConfigurationRetirement{
            allocation.row_index,
            allocation.allocation_generation,
            retire_epoch}) >= 0;
}

void MaterialTextureReferencePool::CollectRetired(
    const uint64_t completed_epoch)
{
    for (int i = 0; i < retirements.GetCount();)
    {
        const MaterialTextureConfigurationRetirement retirement =
            retirements[i];
        if (retirement.retire_epoch > completed_epoch)
        {
            ++i;
            continue;
        }

        if (!ReleaseRow(
                retirement.row_index,
                retirement.allocation_generation))
        {
            GLogError(
                "[MaterialTextureReferencePool] failed to reclaim definition=%s row=%u",
                definition_id.c_str(),
                retirement.row_index);
        }
        retirements.Delete(i);
    }
}
}
