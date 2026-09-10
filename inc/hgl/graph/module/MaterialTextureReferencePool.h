#pragma once

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/type/ValueArray.h>
#include <hgl/type/String.h>
#include <hgl/vk/VKDevice.h>
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

struct MaterialTextureConfigurationRetirement
{
    uint32_t row_index = 0;
    uint64_t allocation_generation = 0;
    uint64_t retire_epoch = 0;
};

inline bool operator==(
    const MaterialTextureConfigurationRetirement &lhs,
    const MaterialTextureConfigurationRetirement &rhs) noexcept
{
    return lhs.row_index == rhs.row_index
        && lhs.allocation_generation == rhs.allocation_generation
        && lhs.retire_epoch == rhs.retire_epoch;
}

class MaterialTextureReferencePool
{
    AnsiString definition_id;
    uint64_t pool_key = 0;
    uint64_t layout_hash = 0;
    uint32_t reference_count = 0;
    uint32_t row_stride = 0;
    uint32_t max_configuration_count = 0;
    uint32_t live_configuration_count = 0;
    DeviceBuffer *buffer = nullptr;
    void *cpu_base = nullptr;
    uint64_t gpu_base = 0;
    uint64_t next_allocation_generation = 1;
    ValueArray<uint32_t> free_rows;
    ValueArray<uint64_t> row_generations;
    ValueArray<MaterialTextureConfigurationRetirement> retirements;

    bool IsOwnedAllocation(
        const MaterialTextureConfigurationAllocation &allocation) const noexcept;
    bool IsRetired(const uint32_t row_index) const noexcept;
    bool ReleaseRow(
        const uint32_t row_index,
        uint64_t allocation_generation);

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
    uint32_t GetRowStride() const noexcept { return row_stride; }
    uint32_t GetConfigurationCapacity() const noexcept
    {
        return max_configuration_count;
    }
    uint32_t GetLiveConfigurationCount() const noexcept
    {
        return live_configuration_count;
    }
    uint64_t GetZeroRowAddress() const noexcept { return gpu_base; }
};
}
