#pragma once

#include <hgl/CoreType.h>
#include <hgl/type/String.h>
#include <hgl/type/ActiveIDManager.h>

#include <cstdint>
#include <vector>

namespace hgl::graph
{

class DeviceBuffer;
class IGPUBuffer;
class VulkanDevice;

/**
 * ActiveRowPool —— 「一种数据类型的行池」通用件（类型擦除）。
 *
 * 包含原则：Buffer 创建与 BufferView 创建分离。
 *   1. 创建期：只按 (行距, 容量) 建 Buffer（HOST_VISIBLE 直写 + BDA 寻址），
 *      **类型 T 在此不出现** —— 故可放进数组按枚举/键遍历创建；
 *   2. 使用期：由 ActiveRowView 把视图关联到本池，按行字节访问（需要结构体时在使用点强转）。
 *
 * 池持有：Buffer + 元数据（CPU/GPU 基址、行距、容量、ssbo_id）+ 行号空间（ActiveIDManager）。
 * 池【不持有】任何视图；行号生命周期 = Acquire/Release。
 *
 * 复用面：材质字段行池、纹理引用行池、L2W/UBO 行池等「固定容量缓冲 + 行号」场景。
 */
class ActiveRowPool
{
public:
    using RowID = uint32_t;
    static constexpr RowID InvalidRowID = ~RowID(0);

protected:

    DeviceBuffer *buffer       = nullptr;
    void         *cpu_base     = nullptr;   ///< 持久映射的宿主基址（行直写）
    uint64_t      gpu_base     = 0;         ///< 设备地址基址（shader 侧 BDA 寻址）
    uint32_t      ssbo_id      = 0;
    uint32_t      row_bytes    = 0;
    uint32_t      row_capacity = 0;
    uint32_t      reserved_rows = 0;        ///< 创建期预留行数（如 行0=零行），不参与分配
    ActiveIDManager ids;                    ///< 行号空间（FIFO 复用；上限 = row_capacity）

    struct PendingRelease
    {
        RowID    row_id = InvalidRowID;
        uint64_t ready_epoch = 0;           ///< completed_epoch >= ready_epoch 后放回可分配
    };

    std::vector<PendingRelease> pending_releases;   ///< 延迟回收队列（retire 语义）

public:

    ActiveRowPool() = default;
    ~ActiveRowPool() { Reset(); }

    ActiveRowPool(const ActiveRowPool &) = delete;
    ActiveRowPool &operator=(const ActiveRowPool &) = delete;

    /**
     * 创建期入口：只建 Buffer（不涉及 T）。
     * 缓冲 = HOST_VISIBLE 整块可持久映射 + DEVICE_ADDRESS（VulkanDevice::CreateArenaBuffer），
     * 建后清零并整块标脏，行号空间重置。
     *
     * @param reserve_rows  创建期预留的行数（从 0 起，例如「行 0 恒为零行」），不参与分配
     * @param bda_align16   取 16 字节对齐的设备地址（纹理引出行用；材质字段行不需要）
     */
    bool Create(VulkanDevice *device,
                const AnsiString &name,
                uint32_t in_row_bytes,
                uint32_t in_capacity,
                uint32_t in_ssbo_id,
                uint32_t reserve_rows = 0,
                bool bda_align16 = false);

    /** 释放 Buffer + 行号空间（回到未创建状态）。 */
    void Reset();

    bool IsReady() const
    {
        return buffer && cpu_base && gpu_base && row_bytes && row_capacity;
    }

    template<typename T>
    bool MatchesRowSize() const { return row_bytes == sizeof(T); }

    // ---- 行号空间 ----

    RowID Acquire();
    bool  Release(RowID id);
    bool  IsActive(RowID id) const;
    uint32_t GetActiveCount() const { return uint32_t(ids.GetActiveCount()); }
    uint32_t GetReservedRowCount() const { return reserved_rows; }

    /**
     * 延迟回收（retire 语义）：该行保持占用，直到 CollectRecyclable(completed_epoch)
     * 且 completed_epoch >= ready_epoch 才清零并放回可分配。
     */
    bool ReleaseDeferred(RowID id, uint64_t ready_epoch);

    /** 回收所有到期的延迟释放行（帧边界调用）；返回回收数。
     *  @param out_recycled 非空时收集被回收的行号（持有者据此清自己的生成号/计数） */
    uint32_t CollectRecyclable(uint64_t completed_epoch,
                               std::vector<RowID> *out_recycled = nullptr);

    /** 该行是否已在延迟回收队列中（仍占用，但即将失效）。 */
    bool IsPendingRelease(RowID id) const
    {
        for (const auto &pending : pending_releases)
        {
            if (pending.row_id == id)
                return true;
        }
        return false;
    }

    uint32_t GetPendingReleaseCount() const
    {
        return static_cast<uint32_t>(pending_releases.size());
    }

    // ---- 行数据 ----

    void *RowCPU(RowID id) const
    {
        return (cpu_base && row_bytes && id < row_capacity)
            ? static_cast<uint8_t *>(cpu_base) + size_t(id) * row_bytes
            : nullptr;
    }

    uint64_t RowGPU(RowID id) const
    {
        return (gpu_base && row_bytes && id < row_capacity)
            ? gpu_base + uint64_t(id) * row_bytes
            : 0;
    }

    /** 按行标脏（提交粒度 = 一行，不整池）。 */
    bool CommitRow(RowID id);

    // ---- 元数据 ----

    DeviceBuffer     *GetBuffer()       const { return buffer; }
    IGPUBuffer       *GetGPUBuffer()    const;
    const IGPUBuffer *GetGPUBufferConst() const;
    void             *GetCPUBase()      const { return cpu_base; }
    uint64_t          GetGPUBase()      const { return gpu_base; }
    uint32_t          GetSSBOId()       const { return ssbo_id; }
    uint32_t          GetRowBytes()     const { return row_bytes; }
    uint32_t          GetRowCapacity()  const { return row_capacity; }
};

} // namespace hgl::graph
