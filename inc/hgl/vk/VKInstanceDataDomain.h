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
    mtl::InstanceDataLayout  instance_layout         = mtl::InstanceDataLayout::None;
    uint32_t                 mi_max_count             = 0;       ///< 渲染批次最大实例数
    uint8_t                  texture_array_slot_flags = 0;       ///< 供方：该域提供哪些 TextureArray slot

    hgl::ActiveMemoryBlockManager *mi_data_manager = nullptr;    ///< 该域独立的 MI 数据池

    // ----------------------------------------------------------------
    // Phase C: domain-owned GPU buffers (transitional)
    // ----------------------------------------------------------------
    BufferManager *buffer_manager = nullptr;
    DeviceBuffer  *mi_gpu_buffer  = nullptr;
    DeviceBuffer  *mit_gpu_buffer = nullptr;

    uint32_t mi_gpu_capacity  = 0;   ///< MI element capacity
    uint32_t mit_gpu_capacity = 0;   ///< MIT uint32 capacity

    bool mi_dirty = false;
    uint32_t mi_dirty_begin = 0;
    uint32_t mi_dirty_end = 0;       ///< one past end

    bool mit_dirty = false;
    uint32_t mit_dirty_begin = 0;
    uint32_t mit_dirty_end = 0;      ///< one past end

    uint64_t mi_uploaded_bytes_total = 0;
    uint64_t mit_uploaded_bytes_total = 0;
    uint32_t mi_full_upload_fallback_count = 0;
    uint32_t mit_full_upload_fallback_count = 0;

private:

    friend class MRDManager;

    explicit InstanceDataDomain(mtl::InstanceDataLayout layout,
                                    uint32_t max_count,
                                    uint8_t tex_array_slots = 0);

public:

    virtual ~InstanceDataDomain();

    // ----------------------------------------------------------------
    // 基础属性查询
    // ----------------------------------------------------------------

    mtl::InstanceDataLayout GetLayout()         const { return instance_layout; }
    bool     hasMI()                            const { return instance_layout != mtl::InstanceDataLayout::None; }
    uint32_t GetMIDataBytes()                   const { return mtl::GetInstanceDataStride(instance_layout); }
    uint32_t GetMIMaxCount()                    const { return mi_max_count; }
    uint8_t  GetTextureArraySlots()             const { return texture_array_slot_flags; }

    // ----------------------------------------------------------------
    // MI 槽位管理 — 仅被 MaterialInstanceData 析构路径使用
    // ----------------------------------------------------------------

    /**
     * 从本域分配一个 MI 槽位。
     * @return 分配到的 mi_id；若本域不承载 MI 数据则返回 -1。
     */
    int  AllocMISlot();

    /**
     * 归还 MI 槽位。
     */
    void FreeMISlot(int mi_id);

    /**
     * 获取指定槽位的原始数据指针。
     */
    void *GetMIData(int mi_id);

    // ----------------------------------------------------------------
    // Phase C (transitional): domain-owned SSBO helpers
    // ----------------------------------------------------------------

    bool EnsureMIBuffer(BufferManager *bm, uint32_t min_mi_count, bool allow_recreate = true);
    bool EnsureMITBuffer(BufferManager *bm, uint32_t min_uint_count, bool allow_recreate = true);

    DeviceBuffer *GetMIGPUBuffer() const { return mi_gpu_buffer; }
    DeviceBuffer *GetMITGPUBuffer() const { return mit_gpu_buffer; }

    uint32_t GetMIGPUCapacity() const { return mi_gpu_capacity; }
    uint32_t GetMITGPUCapacity() const { return mit_gpu_capacity; }

    bool HasMIDirtyRange() const { return mi_dirty; }
    bool HasMITDirtyRange() const { return mit_dirty; }
    uint32_t GetMIDirtyBegin() const { return mi_dirty_begin; }
    uint32_t GetMIDirtyEnd() const { return mi_dirty_end; }
    uint32_t GetMITDirtyBegin() const { return mit_dirty_begin; }
    uint32_t GetMITDirtyEnd() const { return mit_dirty_end; }

    void MarkMIDirtyRange(uint32_t begin, uint32_t count);
    void MarkMITDirtyRange(uint32_t begin, uint32_t count);
    void ClearMIDirtyRange();
    void ClearMITDirtyRange();

    bool UploadMIDirtyRange();
    bool UploadMITDirtyRange(const uint32_t *mit_source_data, uint32_t mit_source_count);

    uint64_t GetMIUploadedBytesTotal() const { return mi_uploaded_bytes_total; }
    uint64_t GetMITUploadedBytesTotal() const { return mit_uploaded_bytes_total; }
    uint32_t GetMIFullUploadFallbackCount() const { return mi_full_upload_fallback_count; }
    uint32_t GetMITFullUploadFallbackCount() const { return mit_full_upload_fallback_count; }

}; // class InstanceDataDomain

} // namespace hgl::graph

