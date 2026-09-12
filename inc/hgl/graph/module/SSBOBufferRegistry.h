#pragma once

#include <hgl/graph/module/GraphModule.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/graph/module/MaterialTextureReferencePool.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/type/ManagedArray.h>
#include <unordered_map>
#include <hgl/log/Log.h>

namespace hgl::graph
{
class DeviceBuffer;
class IGPUBuffer;

struct SSBOBufferBinding
{
    mtl::SSBOType ssbo_type = mtl::SSBOType::UserDefined;
    uint32_t ssbo_id = 0;
    DeviceBuffer *buffer = nullptr;
    uint32_t element_capacity = 0;
    uint32_t element_stride = 0;
};

GRAPH_MODULE_CLASS(SSBOBufferRegistry)
{
private:

    std::unordered_map<uint64_t, SSBOBufferBinding> domain_map;

    ManagedArray<MaterialTextureReferencePool> material_texture_reference_pools;

    DeviceBuffer *null_row_buffer  = nullptr;   ///< 64B 零填充"空行"
    uint64_t      null_row_address = 0;

private:

    SSBOBufferRegistry(GraphicsContext *);
    ~SSBOBufferRegistry() = default;

    friend class GraphModuleManager;

private:

    static uint64_t MakeKey(const mtl::SSBOAddress &address) noexcept;
    SSBOBufferBinding *FindMutable(const mtl::SSBOAddress &address);
    const SSBOBufferBinding *Find(const mtl::SSBOAddress &address) const;
    MaterialTextureReferencePool *FindMaterialTextureReferencePool(
        const mtl::MaterialDefinition &definition,
        const mtl::MaterialTextureReferenceLayout &layout);
    MaterialTextureReferencePool *FindMaterialTextureReferencePool(
        const MaterialTextureConfigurationAllocation &allocation);

public:

    void Release() override;

    bool Touch(const mtl::SSBOAddress &address);

    bool RegisterBuffer(const mtl::SSBOAddress &address, DeviceBuffer *buffer, uint32_t element_capacity = 0);

    bool ClearDomain(const mtl::SSBOAddress &address);

    bool HasBinding(const mtl::SSBOAddress &address) const;
    bool TryGetBinding(const mtl::SSBOAddress &address, SSBOBufferBinding &out_binding) const;

    uint32_t GetElementCapacity(const mtl::SSBOAddress &address) const;

    uint32_t GetCount() const { return static_cast<uint32_t>(domain_map.size()); }

    /**
     * Returns one dynamically-sized texture-reference configuration row from
     * the pool owned by this MaterialDefinition/layout pair. Row zero remains
     * permanently reserved as a safe all-zero fallback.
     */
    bool AcquireMaterialTextureConfiguration(
        const mtl::MaterialDefinition &definition,
        const mtl::MaterialTextureReferenceLayout &layout,
        MaterialTextureConfigurationAllocation &out_allocation);

    /**
     * Writes one complete descriptor-index/array-layer configuration. The
     * reference count must exactly match the definition layout.
     */
    bool WriteMaterialTextureConfiguration(
        const MaterialTextureConfigurationAllocation &allocation,
        const mtl::MaterialTextureReference *references,
        uint32_t reference_count);
    bool IsMaterialTextureConfigurationValid(
        const MaterialTextureConfigurationAllocation &allocation);
    uint64_t GetMaterialTextureConfigurationZeroRowAddress(
        const mtl::MaterialDefinition &definition,
        const mtl::MaterialTextureReferenceLayout &layout);

    /**
     * Defers row reuse until the caller's completed GPU epoch reaches
     * retire_epoch. Phase 4 wires this to MaterialComponent lifecycle.
     */
    bool RetireMaterialTextureConfiguration(
        const MaterialTextureConfigurationAllocation &allocation,
        uint64_t retire_epoch);
    void CollectRetiredMaterialTextureConfigurations(uint64_t completed_epoch);
    uint32_t GetMaterialTextureReferencePoolCount() const
    {
        return static_cast<uint32_t>(
            material_texture_reference_pools.GetCount());
    }

    /**
     * Null 行地址：64B 零填充缓冲（惰性创建），地址行表中
     * "无有效行"条目的安全缺省——任何 BDA 解引用都不会踩非法地址。
     */
    uint64_t GetNullRowAddress();
};
} // namespace hgl::graph
