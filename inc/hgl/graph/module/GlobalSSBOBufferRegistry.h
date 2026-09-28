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
#include <hgl/common/RenderOptions.h>

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
 * - 自身闭环管理全局地址表（GlobalAddresses，SSBO + 设备地址；经 pc_root.addr_global_addresses
 *   下发，无绑定无集），启动时一次性写入，0 运行时 CPU 开销；
 * - 提供类型擦除与泛型并存的统一 RAII 租约访问器 GlobalSSBODataAccessor。
 */
GRAPH_MODULE_CLASS(GlobalSSBOBufferRegistry)
{
private:
    ActiveRowPool pools[GlobalSSBOTypeCount];
    DeviceBuffer *global_addresses_table_buffer = nullptr;
    StructView<GlobalAddresses> *global_addresses_table = nullptr;
    uint64_t      global_addresses_addr = 0;      ///< 表**第 0 槽**基址；第 n 槽 = 基址 + n*槽步长
    bool initialized = false;

    /// CPU 侧镜像：全局字段（所有帧槽相同）+ 每帧槽字段（sky / viewport / shadow 地址）。
    /// 写表时按槽组合 —— 表本体只有一份，槽靠「基址 + 槽号*槽步长」切分。
    GlobalAddresses global_addresses_global{};

    struct GlobalAddressesSlot
    {
        uint64_t sky = 0;
        uint64_t viewport = 0;
        uint64_t shadow = 0;
    };
    GlobalAddressesSlot global_addresses_slots[kGlobalAddressesSlotCount];

    GlobalSSBOBufferRegistry(GraphicsContext *);
    ~GlobalSSBOBufferRegistry() = default;

    friend class GraphModuleManager;

    void OnGraphicsContextChanged(GraphicsContext *) override;
    bool InitializePools();
    bool CreatePool(const GlobalSSBOConfig &config);
    bool InitializeGlobalAddressesTable();

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

    /// 本帧槽的表地址（pc_root.addr_global_addresses 的取址来源；未初始化时为 0）。
    /// 槽号取模 —— 调用方直接送 Context::GetFrameIndex() 或 CameraRow % 槽总数。
    uint64_t GetGlobalAddressesAddress(uint32_t frame_slot) const;

    /// 按「全局字段 + 该槽的 sky/viewport/shadow 地址」组合并写入某帧槽（无变化则跳过）。
    bool CommitSlot(uint32_t frame_slot);
    bool CommitAllSlots();

    /// sky / shadow 的地址（每帧槽各一份）：写入本槽后 pc_root 指过来即可用。
    /// 传 0 表示「本槽暂无该表」（着色器侧读到 0 地址即解引用 0 ⇒ 调用方须保证不读）。
    void SetSkyAddress(uint32_t frame_slot, uint64_t addr);
    /// sky 是单份 buffer（与帧槽无关）⇒ 一个地址写满所有帧槽。
    void SetSkyAddress(uint64_t addr);
    /// viewport 同理是单份 buffer（内容按 pass/RT 覆盖写、地址恒定）⇒ 一个地址写满所有帧槽。
    void SetViewportAddress(uint64_t addr);
    void SetShadowAddress(uint32_t frame_slot, uint64_t addr);

    /// 调色板地址（BDA）：内容长期有效，地址只在 buffer 重建时才变（当前实现不重建）。
    void UpdateColorPaletteAddress(uint64_t addr_color_palette);

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
            // 三条件逐项打出：否则「未就绪」只能靠猜（initialized / 池就绪 / SSBO id）
            GLogError("[GlobalSSBOBufferRegistry] Accessor requested before ready: type=%s"
                      "（initialized=%d pool=%p IsReady=%d ssbo_id=%u）",
                      GetGlobalSSBOTypeName(type),
                      initialized ? 1 : 0, (const void *)pool,
                      pool ? (pool->IsReady() ? 1 : 0) : 0,
                      pool ? pool->GetSSBOId() : 0u);
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

    // 相机行**已下沉世界级**（`CameraInfoStorage`，16 槽 × HGL_FRAME_SLOT_TOTAL 帧槽，0 号槽=本世界默认相机）：
    // 本 registry 不再持有 `GlobalSSBOType::CameraInfo` 行池、全局相机号位图与 8 相机上限
    // （定稿见 doc/world-addresses-and-camera-model-plan.md §2；旧 API AcquireCamera / ReleaseCamera /
    //  CameraRow / WriteCameraRow / WriteCamera 已随之下线，改由 `CameraInfoStorage` 承担）。

    /// 全部行池的 CommitRow 被拒总次数。契约判据：正常运行恒为 0。
    /// 写入被静默拒绝 = 数据根本没到 GPU —— 这类故障不会让画面崩，只会让数据悄悄不对。
    uint64_t GetCommitRejectCount() const
    {
        uint64_t total = 0;

        for (uint32_t i = 0; i < GlobalSSBOTypeCount; ++i)
        {
            const auto *pool = GetPool(static_cast<GlobalSSBOType>(i));

            if (pool)
                total += pool->GetCommitRejectCount();
        }

        return total;
    }
};


} // namespace hgl::graph
