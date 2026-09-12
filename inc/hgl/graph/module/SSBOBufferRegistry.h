#pragma once

#include <hgl/graph/module/GraphModule.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/graph/module/MaterialTextureReferencePool.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/buffer/ArrayView.h>
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

    /**
     * Arena+BDA: session segment registry. ssbo_id -> {block base, slot blocks}.
     * Collect translates the authored data_index (accessor row number) into a
     * global block number (block_base + idx*slot_blocks) via this table.
     */
public:

    struct RowSegmentInfo
    {
        void    *cpu_base  = nullptr;   ///< 映射基址（CPU 行数据/行尾句柄直写）
        uint64_t gpu_base  = 0;         ///< 设备地址基址（地址行表寻址）
        uint32_t row_bytes = 0;         ///< 行距 = sizeof(行结构)
        uint32_t row_capacity = 0;      ///< 此逻辑段可用的行数
        DeviceBuffer *buffer = nullptr; ///< 行缓冲本体（注册表拥有，Release() 释放）
    };

private:

    std::unordered_map<uint32_t, RowSegmentInfo> row_segments;
    ManagedArray<MaterialTextureReferencePool> material_texture_reference_pools;

    DeviceBuffer *null_row_buffer  = nullptr;   ///< 64B 零填充"空行"
    uint64_t      null_row_address = 0;

    uint32_t next_ssbo_id = 1;  ///< 会话内 SSBO ID 自增计数器（MakeRecipeSSBOId 命名空间）

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

    DeviceBuffer *EnsureBuffer(const mtl::SSBOAddress &address,
                               const AnsiString &name,
                               VkDeviceSize byte_size,
                               uint32_t required_capacity,
                               SharingMode sm = SharingMode::Exclusive);

    bool ClearDomain(const mtl::SSBOAddress &address);

    bool HasBinding(const mtl::SSBOAddress &address) const;
    bool TryGetBinding(const mtl::SSBOAddress &address, SSBOBufferBinding &out_binding) const;

    DeviceBuffer *GetBuffer(const mtl::SSBOAddress &address) const;

    const IGPUBuffer *GetGPUBuffer(const mtl::SSBOAddress &address) const;

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
     * Arena+BDA: query the segment info registered for an accessor ssbo_id.
     * Returns false when the id was not allocated through the arena backend.
     */
    bool TryGetRowSegment(uint32_t ssbo_id, RowSegmentInfo &out_info) const
    {
        const auto it = row_segments.find(ssbo_id);
        if (it == row_segments.end())
            return false;
        out_info = it->second;
        return true;
    }

    /**
     * Null 行地址：64B 零填充缓冲（惰性创建），地址行表中
     * "无有效行"条目的安全缺省——任何 BDA 解引用都不会踩非法地址。
     */
    uint64_t GetNullRowAddress();

    /**
     * CN: 分配一个新的、在本会话内唯一的 SSBO ID（MakeRecipeSSBOId 命名空间）
     *     ID 由内部计数器自增产生，外部不需要也不应该自行编号。
     * EN: Allocate a fresh session-unique SSBO ID (MakeRecipeSSBOId namespace).
     *     ID is produced by an internal counter; callers must not pre-assign IDs.
     */
    uint32_t AllocateSSBOId();

    /**
     * CN: 一步式"申请 SSBO ID + 创建缓冲区 + 包装访问器"
     *
     *     正确的使用流程：
     *       auto* acc = domain_manager->AllocateArrayAccessor<Color4f>(
     *                       SSBOType::UserDefined, "MySSBO", count);
     *       // 直接用 accessor 的 type+id 注册 recipe 绑定
     *
     * EN: One-step "allocate SSBO ID + create buffer + wrap accessor".
     *     The allocated ID is stored inside the accessor; retrieve it via acc->GetSSBOId().
     *
     *     此接口仅用于通用运行时 SSBO。材质 payload 行必须由
     *     MaterialSSBOBufferRegistry::GetMaterialDataAccessor<T>() 申请，
     *     并用显式 MaterialSSBOType 建立 recipe 绑定。
     *
     * @param ssbo_type     SSBO 类型
     * @param name          缓冲区调试名称
     * @param element_count 数组元素个数
     * @param sm            共享模式，默认 Exclusive
     * @return 成功返回已 Map 的访问器指针（调用方负责 delete），失败返回 nullptr
     */
    template<typename T>
    ArrayView<T>* AllocateArrayAccessor(
        const mtl::SSBOType  ssbo_type,
        const AnsiString&    name,
        uint32_t             element_count,
        SharingMode          sm = SharingMode::Exclusive)
    {
        if (element_count == 0)
            return nullptr;

        const uint32_t allocated_id = AllocateSSBOId();

        // Allocate one independent BDA-capable runtime buffer.
        VulkanDevice *device = GetDevice();
        DeviceBuffer *buf = device
            ? device->CreateArenaBuffer(name, VkDeviceSize(sizeof(T)) * element_count)
            : nullptr;
        if (!buf)
            return nullptr;

        void *cpu_base = buf->GetGPUBuffer()->Map(0, VkDeviceSize(sizeof(T)) * element_count);
        const uint64_t gpu_base = device->GetBufferDeviceAddress(buf->GetBuffer());
        if (!cpu_base || gpu_base == 0)
        {
            delete buf;
            return nullptr;
        }

        // 默认行保障：整缓冲清零（行数据与行尾句柄全零 = 安全缺省）
        memset(cpu_base, 0, size_t(sizeof(T)) * element_count);

        // 视图不拥有数据源（B-2）：行缓冲归注册表，accessor 只在宿主窗口上写。
        // 地址行表里的行地址指向这块缓冲，因此它的寿命 = 注册表寿命。
        auto *acc = new ArrayView<T>(cpu_base, element_count, uint32(sizeof(T)));
        acc->ssbo_id   = allocated_id;
        acc->ssbo_type = ssbo_type;

        RowSegmentInfo seg;
        seg.cpu_base  = cpu_base;
        seg.gpu_base  = gpu_base;
        seg.row_bytes = uint32(sizeof(T));
        seg.row_capacity = element_count;
        seg.buffer    = buf;
        row_segments.emplace(allocated_id, seg);

        return acc;
    }

protected:

protected:

    /**
     * CN: EnsureArrayAccessor — 引擎内部接口，显式指定 SSBOAddress（固定 ID 场景）。
     *     外部应用代码必须使用 AllocateArrayAccessor，不得绕过 ID 分配机制。
     * EN: EnsureArrayAccessor — engine-internal interface with explicit SSBOAddress (fixed-ID use cases).
     *     External application code MUST use AllocateArrayAccessor; do NOT bypass ID allocation.
     */
    template<typename T>
    ArrayView<T>* EnsureArrayAccessor(
        const mtl::SSBOAddress &address,
        const AnsiString       &name,
        uint32_t                element_count,
        SharingMode             sm = SharingMode::Exclusive)
    {
        if (element_count == 0)
            return nullptr;

        DeviceBuffer *buf = EnsureBuffer(address, name,
                                         static_cast<VkDeviceSize>(sizeof(T)) * element_count,
                                         element_count,
                                         sm);
        if (!buf)
            return nullptr;

        auto *acc = ArrayView<T>::Create(buf, element_count);
        if (acc)
        {
            acc->ssbo_id   = address.ssbo_id;
            acc->ssbo_type = address.ssbo_type;
        }

        return acc;
    }
};
} // namespace hgl::graph
