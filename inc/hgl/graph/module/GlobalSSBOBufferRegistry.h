#pragma once

#include <hgl/log/Log.h>
#include <hgl/graph/module/GraphModule.h>
#include <hgl/graph/ssbo/GlobalSSBOTypes.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/graph/ShaderBufferSources.h>
#include <hgl/graph/ubo/GlobalAddresses.h>
#include <hgl/vk/buffer/ActiveRowPool.h>
#include <hgl/vk/buffer/ActiveRowLease.h>
#include <hgl/vk/buffer/StructView.h>

#include <hgl/graph/CameraInfo.h>

namespace hgl::graph
{

class DeviceBuffer;
class IGPUBuffer;
class GlobalSSBOBufferRegistry;

/**
 * GlobalSSBODataAccessor —— 全局 SSBO 行的 RAII 访问器。
 *
 * = 通用行租约 ActiveRowLease（关联池 + 申请行号 + 析构自动归还）
 *   + 全局类型与 SSBO 标识（GlobalSSBOType / ssbo_id）。
 *
 * 既支持材质字段（PBRSurface / EmissiveSurface / TransmissionSurface），
 * 也支持几何绘制参数（MeshDrawParams）。
 */
class GlobalSSBODataAccessor : public ActiveRowLease
{
    GlobalSSBOType global_ssbo_type = GlobalSSBOType::MeshDrawParams;
    uint32_t ssbo_id = 0;

    GlobalSSBODataAccessor(
        ActiveRowPool *pool,
        const GlobalSSBOType in_type,
        const uint32_t in_ssbo_id)
        : ActiveRowLease(pool)
        , global_ssbo_type(in_type)
        , ssbo_id(in_ssbo_id)
    {
    }

    friend class GlobalSSBOBufferRegistry;

public:
    GlobalSSBODataAccessor() = default;
    GlobalSSBODataAccessor(GlobalSSBODataAccessor &&) noexcept = default;
    GlobalSSBODataAccessor &operator=(GlobalSSBODataAccessor &&) noexcept = default;

    uint32_t GetDataID() const { return GetRowID(); }
    uint32_t GetSSBOId() const { return ssbo_id; }
    GlobalSSBOType GetGlobalSSBOType() const { return global_ssbo_type; }

    GlobalSSBOBinding GetGlobalSSBOBinding() const
    {
        return {global_ssbo_type, ssbo_id, GetRowID()};
    }
};

struct GlobalRowBufferInfo
{
    GlobalSSBOType global_ssbo_type = GlobalSSBOType::PBRSurface;
    void *cpu_base = nullptr;
    uint64_t gpu_base = 0;
    uint32_t row_bytes = 0;
    uint32_t row_capacity = 0;
    DeviceBuffer *buffer = nullptr;
};

/**
 * GlobalSSBOBufferRegistry —— 全局单一数组 SSBO 行池统一注册管理器。
 *
 * 统一管理通过全局固定上限 Arena 缓冲（持久 BDA 寻址）托管的数据池：
 * 1. 几何绘制参数（MeshDrawParams，112B）；
 * 2. 材质表面字段（PBRSurface 32B / EmissiveSurface 16B / TransmissionSurface 16B）。
 *
 * 特性：
 * - 启动时表驱动一次性分配所有 Arena Buffer，终身不重建，BDA 终身恒定；
 * - 自身闭环管理 Set 0 Binding 4 的 GlobalAddressesInfo UBO（启动时一次性写入，0 运行时 CPU 开销）；
 * - 提供类型擦除与泛型并存的统一 RAII 租约访问器 GlobalSSBODataAccessor。
 */
GRAPH_MODULE_CLASS(GlobalSSBOBufferRegistry)
{
private:
    ActiveRowPool pools[GlobalSSBOTypeCount];
    DeviceBuffer *global_addresses_ubo_buffer = nullptr;
    StructView<GlobalAddresses> *global_addresses_ubo = nullptr;
    bool initialized = false;

    GlobalSSBOBufferRegistry(GraphicsContext *);
    ~GlobalSSBOBufferRegistry() = default;

    friend class GraphModuleManager;

    void OnGraphicsContextChanged(GraphicsContext *) override;
    bool InitializePools();
    bool CreatePool(const GlobalSSBOConfig &config);
    bool InitializeGlobalAddressesUBO();

public:
    void Release() override;

    bool IsInitialized() const { return initialized; }

    ActiveRowPool *GetPool(GlobalSSBOType type)
    {
        if (!IsGlobalSSBOType(type))
            return nullptr;
        return &pools[static_cast<uint32_t>(type)];
    }

    const ActiveRowPool *GetPool(GlobalSSBOType type) const
    {
        if (!IsGlobalSSBOType(type))
            return nullptr;
        return &pools[static_cast<uint32_t>(type)];
    }

    uint64_t GetGPUBase(GlobalSSBOType type) const
    {
        const auto *pool = GetPool(type);
        return pool ? pool->GetGPUBase() : 0;
    }

    const IGPUBuffer *GetGlobalAddressesUBO() const
    {
        return global_addresses_ubo ? global_addresses_ubo->GetGPUBuffer() : nullptr;
    }

    void UpdateRenderItemAddresses(uint64_t addr_render_items, uint64_t addr_draw_item_ids);

    uint32_t Acquire(GlobalSSBOType type)
    {
        auto *pool = GetPool(type);
        return pool ? pool->Acquire() : ActiveRowPool::InvalidRowID;
    }

    bool ReleaseID(GlobalSSBOType type, uint32_t id)
    {
        auto *pool = GetPool(type);
        return pool ? pool->Release(id) : false;
    }

    bool Write(GlobalSSBOType type, uint32_t id, const void *data, uint32_t bytes)
    {
        auto *pool = GetPool(type);
        if (!pool || !data || bytes == 0)
            return false;
        void *dst = pool->RowCPU(id);
        if (!dst || bytes > pool->GetRowBytes())
            return false;
        memcpy(dst, data, bytes);
        return pool->CommitRow(id);
    }

    template<typename T>
    bool Write(GlobalSSBOType type, uint32_t id, const T &val)
    {
        return Write(type, id, &val, sizeof(T));
    }

    bool IsActive(GlobalSSBOType type, uint32_t id) const
    {
        const auto *pool = GetPool(type);
        return pool ? pool->IsActive(id) : false;
    }

    DeviceBuffer *GetBuffer(GlobalSSBOType type) const
    {
        const auto *pool = GetPool(type);
        return pool ? pool->GetBuffer() : nullptr;
    }

    uint32_t GetCapacity(GlobalSSBOType type) const
    {
        const auto *pool = GetPool(type);
        return pool ? pool->GetRowCapacity() : 0;
    }

    uint32_t GetActiveCount(GlobalSSBOType type) const
    {
        const auto *pool = GetPool(type);
        return pool ? pool->GetActiveCount() : 0;
    }

    GlobalSSBODataAccessor GetAccessor(GlobalSSBOType type)
    {
        auto *pool = GetPool(type);
        if (!initialized || !pool || !pool->IsReady() || pool->GetSSBOId() == 0)
        {
            GLogError("[GlobalSSBOBufferRegistry] Accessor requested before ready: type=%s",
                      GetGlobalSSBOTypeName(type));
            return {};
        }
        return GlobalSSBODataAccessor(pool, type, pool->GetSSBOId());
    }

    template<typename T>
    GlobalSSBODataAccessor GetAccessor()
    {
        GlobalSSBODataAccessor accessor =
            GetAccessor(ssbo::GlobalRowTypeTraits<T>::TYPE);

        if (accessor && accessor.GetRowBytes() != sizeof(T))
        {
            GLogError(
                "[GlobalSSBOBufferRegistry] Material accessor row-size mismatch: type=%s expected=%u actual=%zu",
                GetGlobalSSBOTypeName(ssbo::GlobalRowTypeTraits<T>::TYPE),
                accessor.GetRowBytes(),
                sizeof(T));
            return {};
        }

        return accessor;
    }

    bool TryGetRowBuffer(uint32_t ssbo_id, GlobalRowBufferInfo &out_info) const;

    // ---- MeshDrawParams 便捷接口（平滑过渡 GlobalSSBOBufferRegistry） ----

    uint32_t Acquire(const mtl::MeshDrawParams &params)
    {
        uint32_t id = Acquire(GlobalSSBOType::MeshDrawParams);
        if (id != ActiveRowPool::InvalidRowID)
            Write(GlobalSSBOType::MeshDrawParams, id, params);
        return id;
    }

    bool Write(uint32_t id, const mtl::MeshDrawParams &params)
    {
        return Write(GlobalSSBOType::MeshDrawParams, id, params);
    }

    bool ReleaseID(uint32_t id)
    {
        return ReleaseID(GlobalSSBOType::MeshDrawParams, id);
    }

    uint64_t GetMeshDrawParamsGPUBase() const
    {
        return GetGPUBase(GlobalSSBOType::MeshDrawParams);
    }

    uint64_t GetRowGPU(uint32_t id) const
    {
        const auto *pool = GetPool(GlobalSSBOType::MeshDrawParams);
        return pool ? pool->RowGPU(id) : 0;
    }

    DeviceBuffer *GetMeshDrawParamsBuffer() const
    {
        return GetBuffer(GlobalSSBOType::MeshDrawParams);
    }

    // ---- CameraInfo 便捷接口 ----

    uint32_t AcquireCamera()
    {
        return Acquire(GlobalSSBOType::CameraInfo);
    }

    uint32_t AcquireCamera(const CameraInfo &info)
    {
        uint32_t id = Acquire(GlobalSSBOType::CameraInfo);
        if (id != ActiveRowPool::InvalidRowID)
            Write(GlobalSSBOType::CameraInfo, id, &info, sizeof(CameraInfo));
        return id;
    }

    bool WriteCamera(uint32_t id, const CameraInfo &info)
    {
        return Write(GlobalSSBOType::CameraInfo, id, &info, sizeof(CameraInfo));
    }

    bool ReleaseCamera(uint32_t id)
    {
        return ReleaseID(GlobalSSBOType::CameraInfo, id);
    }

    uint64_t GetCameraInfoGPUBase() const
    {
        return GetGPUBase(GlobalSSBOType::CameraInfo);
    }

    DeviceBuffer *GetCameraInfoBuffer() const
    {
        return GetBuffer(GlobalSSBOType::CameraInfo);
    }
};


} // namespace hgl::graph
