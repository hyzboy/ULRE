#pragma once

#include <hgl/graph/module/GraphModule.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/vk/SSBOArrayAccessor.h>
#include <unordered_map>

namespace hgl::graph
{
class DeviceBuffer;
class IGPUBuffer;

struct ResourceDomainBinding
{
    mtl::SSBOType ssbo_type = mtl::SSBOType::UserDefined;
    uint32_t ssbo_id = 0;
    DeviceBuffer *buffer = nullptr;
    uint32_t element_capacity = 0;
    uint32_t element_stride = 0;
};

GRAPH_MODULE_CLASS(ResourceDomainManager)
{
private:

    std::unordered_map<uint64_t, ResourceDomainBinding> domain_map;

    uint32_t next_ssbo_id = 1;  ///< 会话内 SSBO ID 自增计数器（MakeRecipeSSBOId 命名空间）

private:

    ResourceDomainManager(GraphicsContext *);
    ~ResourceDomainManager() = default;

    friend class GraphModuleManager;

private:

    static uint64_t MakeKey(const mtl::SSBOAddress &address) noexcept;
    ResourceDomainBinding *FindMutable(const mtl::SSBOAddress &address);
    const ResourceDomainBinding *Find(const mtl::SSBOAddress &address) const;

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
    bool TryGetBinding(const mtl::SSBOAddress &address, ResourceDomainBinding &out_binding) const;

    DeviceBuffer *GetBuffer(const mtl::SSBOAddress &address) const;

    const IGPUBuffer *GetGPUBuffer(const mtl::SSBOAddress &address) const;

    uint32_t GetElementCapacity(const mtl::SSBOAddress &address) const;

    uint32_t GetCount() const { return static_cast<uint32_t>(domain_map.size()); }

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
     *                       SSBOType::PBRSurface, "MySSBO", count);
     *       // 直接用 accessor 的 type+id 注册 recipe 绑定
     *       UpsertRecipeSSBOAssetBinding(recipe, name, acc->GetSSBOBinding());
     *
     * EN: One-step "allocate SSBO ID + create buffer + wrap accessor".
     *     The allocated ID is stored inside the accessor; retrieve it via acc->GetSSBOId().
     *
     * @param ssbo_type     SSBO 类型
     * @param name          缓冲区调试名称
     * @param element_count 数组元素个数
     * @param sm            共享模式，默认 Exclusive
     * @return 成功返回已 Map 的访问器指针（调用方负责 delete），失败返回 nullptr
     */
    template<typename T>
    SSBOArrayAccessor<T>* AllocateArrayAccessor(
        const mtl::SSBOType  ssbo_type,
        const AnsiString&    name,
        uint32_t             element_count,
        SharingMode          sm = SharingMode::Exclusive)
    {
        if (element_count == 0)
            return nullptr;

        const uint32_t allocated_id = AllocateSSBOId();

        DeviceBuffer *buf = EnsureBuffer(
            mtl::SSBOAddress{ssbo_type, allocated_id, 0},
            name,
            static_cast<VkDeviceSize>(sizeof(T)) * element_count,
            element_count,
            sm);

        if (!buf)
            return nullptr;

        auto *acc = SSBOArrayAccessor<T>::Create(buf, element_count);
        if (acc)
        {
            acc->ssbo_id   = allocated_id;  // 写入内部存储的 ID
            acc->ssbo_type = ssbo_type;     // 写入内部存储的类型
        }

        return acc;
    }

protected:

    /**
     * CN: EnsureArrayAccessor — 引擎内部接口，显式指定 SSBOAddress（固定 ID 场景）。
     *     外部应用代码必须使用 AllocateArrayAccessor，不得绕过 ID 分配机制。
     * EN: EnsureArrayAccessor — engine-internal interface with explicit SSBOAddress (fixed-ID use cases).
     *     External application code MUST use AllocateArrayAccessor; do NOT bypass ID allocation.
     */
    template<typename T>
    SSBOArrayAccessor<T>* EnsureArrayAccessor(
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

        auto *acc = SSBOArrayAccessor<T>::Create(buf, element_count);
        if (acc)
        {
            acc->ssbo_id   = address.ssbo_id;
            acc->ssbo_type = address.ssbo_type;
        }

        return acc;
    }
};
} // namespace hgl::graph
