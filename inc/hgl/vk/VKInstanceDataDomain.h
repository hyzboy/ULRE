#pragma once

#include <hgl/mtl/InstanceDataLayout.h>

#include <cstdint>

namespace hgl
{
    class ActiveMemoryBlockManager;
}

namespace hgl::graph
{

class BufferManager;
class DeviceBuffer;

/**
 * 资源域 (InstanceDataDomain)
 *
 * 持有与特定 InstanceDataLayout 兼容的独立 MaterialInstance 数据池，同时声明该域
 * 提供的 TextureArray slot 集合（供方）。
 * 同一套 Shader/GraphicsPipeline 可关联多个 InstanceDataDomain，使不同的资源集合
 * （例如UI图标 vs 角色头像 Billboard）彼此隔离，互不串绑。
 */
class InstanceDataDomain
{
    mtl::InstanceDataLayout  instance_layout            = mtl::InstanceDataLayout::None;
    uint32_t                 max_count                  = 0;        ///< 总计可分配的实例数据数量
    uint8_t                  texture_array_slot_flags   = 0;        ///< 供方：该域提供哪些 TextureArray slot

    hgl::ActiveMemoryBlockManager *data_manager = nullptr;      ///< 该域独立的数据池

    // ----------------------------------------------------------------
    // Phase C: domain-owned GPU buffers (transitional)
    // ----------------------------------------------------------------
    BufferManager *buffer_manager = nullptr;

    DeviceBuffer  *gpu_buffer  = nullptr;

    uint32_t gpu_capacity  = 0;   ///< data element capacity

    bool dirty = false;
    uint32_t dirty_begin = 0;
    uint32_t dirty_end = 0;       ///< one past end

    uint64_t uploaded_bytes_total = 0;
    uint32_t full_upload_fallback_count = 0;

private:

    friend class IDDManager;

    explicit InstanceDataDomain(mtl::InstanceDataLayout layout,
                                    uint32_t max_count,
                                    uint8_t tex_array_slots = 0);

public:

    virtual ~InstanceDataDomain();

    // ----------------------------------------------------------------
    // 基础属性查询
    // ----------------------------------------------------------------

    mtl::InstanceDataLayout GetLayout()         const { return instance_layout; }
    bool     HasLayout()                        const { return instance_layout != mtl::InstanceDataLayout::None; }
    uint32_t GetDataStride()                    const { return mtl::GetInstanceDataStride(instance_layout); }
    uint32_t GetMaxCount()                      const { return max_count; }
    uint8_t  GetTextureArraySlots()             const { return texture_array_slot_flags; }

    // ----------------------------------------------------------------
    // MI 槽位管理 — 仅被 MaterialInstanceData 析构路径使用
    // ----------------------------------------------------------------

    /**
     * 从本域分配一个槽位。
     * @return 分配到的槽位 id；若本域不承载数据则返回 -1。
     */
    int  AllocSlot();

    /**
     * 归还槽位。
     */
    void FreeSlot(int slot_id);

    /**
     * 获取指定槽位的原始数据指针。
     */
    void *GetSlotData(int slot_id);

    // ----------------------------------------------------------------
    // Phase C (transitional): domain-owned SSBO helpers
    // ----------------------------------------------------------------

    bool EnsureGPUBuffer(BufferManager *bm, uint32_t min_mi_count, bool allow_recreate = true);

    DeviceBuffer *GetGPUBuffer() const { return gpu_buffer; }

    uint32_t GetGPUCapacity() const { return gpu_capacity; }

    bool HasDirtyRange() const { return dirty; }
    uint32_t GetDirtyBegin() const { return dirty_begin; }
    uint32_t GetDirtyEnd() const { return dirty_end; }

    void MarkDirtyRange(uint32_t begin, uint32_t count);
    void ClearDirtyRange();

    bool UploadDirtyRange();

    uint64_t GetUploadedBytesTotal() const { return uploaded_bytes_total; }
    uint32_t GetFullUploadFallbackCount() const { return full_upload_fallback_count; }
}; // class InstanceDataDomain

} // namespace hgl::graph

