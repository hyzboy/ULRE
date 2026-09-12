#pragma once

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/type/ValueArray.h>
#include <hgl/type/String.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/buffer/ActiveRowPool.h>
#include <hgl/vk/buffer/ActiveRowView.h>
#include <cstdint>

namespace hgl::graph
{
class DeviceBuffer;

constexpr uint64_t MaterialTextureConfigurationRetireEpochDelay = 3u;

struct MaterialTextureConfigurationAllocation
{
    uint64_t pool_key = 0;
    uint32_t row_index = 0;
    uint32_t reference_count = 0;
    uint32_t row_stride = 0;
    uint64_t allocation_generation = 0;
    void *cpu_row = nullptr;
    uint64_t gpu_row = 0;

    bool IsValid() const noexcept
    {
        return pool_key != 0
            && row_index != 0
            && reference_count != 0
            && row_stride != 0
            && allocation_generation != 0
            && cpu_row != nullptr
            && gpu_row != 0;
    }
};

/**
 * MaterialTextureReferencePool —— 每个 (definition, layout) 一份的**字节行池**。
 *
 * 行 = 「该材质的纹理引用列表」（uvec2[reference_count]，元数随材质变化、
 * 行距运行时才知道）——**没有 C++ 结构体**，故内部换装通用件：
 *   - `ActiveRowPool`：Buffer + 行号空间（物理行 = max+1，**行 0 预留为零行**）
 *   - `ActiveRowView`：行视图（按字节访问：整行写 / 偏移写 / 类型化便利）
 *   - 延迟回收走池的 `ReleaseDeferred`/`CollectRecyclable`（retire epoch 由调用方算好）
 * 本类只保留池外的语义：pool_key/layout 匹配、每行分配代（陈旧分配检测）、活跃计数。
 */
class MaterialTextureReferencePool
{
    AnsiString definition_id;
    uint64_t pool_key = 0;
    uint64_t layout_hash = 0;
    uint32_t reference_count = 0;
    uint32_t max_configuration_count = 0;
    uint32_t live_configuration_count = 0;
    uint64_t next_allocation_generation = 1;

    ActiveRowPool row_pool;                 ///< 行池：Buffer + 行号空间；行 0 = 预留零行
    ActiveRowView row_view;                 ///< 行池上的行视图（按字节访问）
    ValueArray<uint64_t> row_generations;   ///< 每行的分配代（0 = 无主）

    bool IsOwnedAllocation(
        const MaterialTextureConfigurationAllocation &allocation) const noexcept;

public:

    MaterialTextureReferencePool() = default;
    ~MaterialTextureReferencePool();

    MaterialTextureReferencePool(const MaterialTextureReferencePool &) = delete;
    MaterialTextureReferencePool &operator=(
        const MaterialTextureReferencePool &) = delete;

    static uint64_t MakePoolKey(
        const mtl::MaterialDefinition &definition,
        const mtl::MaterialTextureReferenceLayout &layout) noexcept
    {
        if (definition.definition_id.empty()
         || layout.layout_hash == 0
         || !layout.HasReferences()
         || layout.row_stride == 0
         || layout.max_configuration_count == 0)
            return 0;

        hgl::hash::FNV1aHasher64 hasher;
        hasher << definition.definition_id
               << layout.layout_hash
               << layout.max_configuration_count;
        return hasher;
    }

    bool Matches(
        const mtl::MaterialDefinition &definition,
        const mtl::MaterialTextureReferenceLayout &layout) const noexcept;
    bool Initialize(
        VulkanDevice *device,
        const mtl::MaterialDefinition &definition,
        const mtl::MaterialTextureReferenceLayout &layout);
    void Release();

    bool Acquire(MaterialTextureConfigurationAllocation &out_allocation);
    bool Write(
        const MaterialTextureConfigurationAllocation &allocation,
        const mtl::MaterialTextureReference *references,
        uint32_t reference_count);
    bool Retire(
        const MaterialTextureConfigurationAllocation &allocation,
        uint64_t retire_epoch);
    void CollectRetired(uint64_t completed_epoch);
    bool IsValidAllocation(
        const MaterialTextureConfigurationAllocation &allocation)
        const noexcept;

    uint64_t GetPoolKey() const noexcept { return pool_key; }
    uint64_t GetLayoutHash() const noexcept { return layout_hash; }
    uint32_t GetReferenceCount() const noexcept { return reference_count; }
    uint32_t GetRowStride() const noexcept { return row_pool.GetRowBytes(); }
    uint32_t GetConfigurationCapacity() const noexcept
    {
        return max_configuration_count;
    }
    uint32_t GetLiveConfigurationCount() const noexcept
    {
        return live_configuration_count;
    }
    uint64_t GetZeroRowAddress() const noexcept { return row_pool.RowGPU(0); }
};
}
