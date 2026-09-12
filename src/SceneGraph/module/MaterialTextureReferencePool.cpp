#include <hgl/graph/module/MaterialTextureReferencePool.h>
#include <hgl/log/Log.h>

#include <cstring>
#include <vector>

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
        && row_pool.GetRowBytes() == layout.row_stride
        && max_configuration_count == layout.max_configuration_count;
}

bool MaterialTextureReferencePool::Initialize(
    VulkanDevice *device,
    const mtl::MaterialDefinition &definition,
    const mtl::MaterialTextureReferenceLayout &layout)
{
    if (row_pool.IsReady() || !device || !layout.HasReferences()
     || layout.row_stride == 0
     || layout.max_configuration_count == 0
     || layout.max_configuration_count
        >= static_cast<uint32_t>(hgl::HGL_S32_MAX))
        return false;

    const uint64_t key = MakePoolKey(definition, layout);
    if (key == 0)
        return false;

    // 物理行 = 配置行 + 1（行 0 恒为零行，永不分配）
    const uint64_t physical_row_count =
        static_cast<uint64_t>(layout.max_configuration_count) + 1u;
    if (physical_row_count > static_cast<uint64_t>(INT_MAX)
     || static_cast<uint64_t>(layout.row_stride)
        > ~uint64_t(0) / physical_row_count)
        return false;

    ValueArray<uint64_t> new_row_generations;
    if (!new_row_generations.Resize(static_cast<int>(physical_row_count)))
        return false;

    const AnsiString buffer_name =
        AnsiString("MaterialTextureReferences:")
        + AnsiString(definition.definition_id.c_str());

    if (!row_pool.Create(device,
                         buffer_name,
                         layout.row_stride,
                         static_cast<uint32_t>(physical_row_count),
                         0,      // ssbo_id：本池不使用 SSBO id 语义
                         1,      // reserve_rows：行 0 = 零行
                         true))  // bda_align16：纹理引用行取 16B 对齐设备地址
        return false;

    if (!row_view.Attach(&row_pool))
    {
        row_pool.Reset();
        return false;
    }

    definition_id = definition.definition_id.c_str();
    pool_key = key;
    layout_hash = layout.layout_hash;
    reference_count = layout.reference_count;
    max_configuration_count = layout.max_configuration_count;
    live_configuration_count = 0;
    next_allocation_generation = 1;
    row_generations = new_row_generations;

    GLogInfo(
        "[MaterialTextureReferencePool] created definition=%s references=%u row_stride=%u capacity=%u bytes=%llu",
        definition_id.c_str(),
        reference_count,
        layout.row_stride,
        max_configuration_count,
        static_cast<unsigned long long>(
            static_cast<uint64_t>(layout.row_stride) * physical_row_count));
    return true;
}

void MaterialTextureReferencePool::Release()
{
    row_view.Detach();
    row_pool.Reset();

    definition_id = "";
    pool_key = 0;
    layout_hash = 0;
    reference_count = 0;
    max_configuration_count = 0;
    live_configuration_count = 0;
    next_allocation_generation = 1;
    row_generations.Free();
}

bool MaterialTextureReferencePool::IsOwnedAllocation(
    const MaterialTextureConfigurationAllocation &allocation) const noexcept
{
    if (!allocation.IsValid()
     || allocation.pool_key != pool_key
     || allocation.row_index > max_configuration_count
     || allocation.reference_count != reference_count
     || allocation.row_stride != row_pool.GetRowBytes()
     || allocation.allocation_generation
        != row_generations[static_cast<int>(allocation.row_index)]
     || !row_pool.IsReady())
        return false;

    return allocation.cpu_row == row_pool.RowCPU(allocation.row_index)
        && allocation.gpu_row == row_pool.RowGPU(allocation.row_index);
}

bool MaterialTextureReferencePool::Acquire(
    MaterialTextureConfigurationAllocation &out_allocation)
{
    out_allocation = {};
    if (!row_pool.IsReady())
    {
        GLogError(
            "[MaterialTextureReferencePool] acquire rejected: pool not ready definition=%s",
            definition_id.c_str());
        return false;
    }

    const ActiveRowPool::RowID row_index = row_pool.Acquire();
    if (row_index == ActiveRowPool::InvalidRowID)
    {
        GLogError(
            "[MaterialTextureReferencePool] exhausted definition=%s live=%u capacity=%u",
            definition_id.c_str(),
            live_configuration_count,
            max_configuration_count);
        return false;
    }

    void *row = row_pool.RowCPU(row_index);
    if (!row)
    {
        row_pool.Release(row_index);
        return false;
    }

    memset(row, 0, row_pool.GetRowBytes());

    uint64_t allocation_generation = next_allocation_generation++;
    if (allocation_generation == 0)
        allocation_generation = next_allocation_generation++;
    row_generations[static_cast<int>(row_index)] = allocation_generation;

    ++live_configuration_count;
    out_allocation.pool_key = pool_key;
    out_allocation.row_index = row_index;
    out_allocation.reference_count = reference_count;
    out_allocation.row_stride = row_pool.GetRowBytes();
    out_allocation.allocation_generation = allocation_generation;
    out_allocation.cpu_row = row;
    out_allocation.gpu_row = row_pool.RowGPU(row_index);
    return true;
}

bool MaterialTextureReferencePool::Write(
    const MaterialTextureConfigurationAllocation &allocation,
    const mtl::MaterialTextureReference *references,
    const uint32_t in_reference_count)
{
    if (!IsOwnedAllocation(allocation)
     || row_pool.IsPendingRelease(allocation.row_index)
     || !references
     || in_reference_count != reference_count)
        return false;

    const uint32_t byte_count = static_cast<uint32_t>(
        static_cast<size_t>(reference_count)
            * sizeof(mtl::MaterialTextureReference));

    // 整行写（先清零整行再拷贝）+ 按行提交
    return row_view.WriteRow(allocation.row_index, references, byte_count);
}

bool MaterialTextureReferencePool::IsValidAllocation(
    const MaterialTextureConfigurationAllocation &allocation)
    const noexcept
{
    return IsOwnedAllocation(allocation)
        && !row_pool.IsPendingRelease(allocation.row_index);
}

bool MaterialTextureReferencePool::Retire(
    const MaterialTextureConfigurationAllocation &allocation,
    const uint64_t retire_epoch)
{
    if (!IsOwnedAllocation(allocation)
     || row_pool.IsPendingRelease(allocation.row_index))
        return false;

    // retire_epoch 由调用方算好（现在 + MaterialTextureConfigurationRetireEpochDelay）
    return row_pool.ReleaseDeferred(allocation.row_index, retire_epoch);
}

void MaterialTextureReferencePool::CollectRetired(
    const uint64_t completed_epoch)
{
    std::vector<ActiveRowPool::RowID> recycled;
    const uint32_t collected =
        row_pool.CollectRecyclable(completed_epoch, &recycled);

    if (collected == 0)
        return;

    for (const auto row_index : recycled)
    {
        if (row_index < static_cast<uint32_t>(row_generations.GetCount()))
            row_generations[static_cast<int>(row_index)] = 0;
    }

    live_configuration_count = (live_configuration_count >= collected)
        ? (live_configuration_count - collected)
        : 0;
}
}
