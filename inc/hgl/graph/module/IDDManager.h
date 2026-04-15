#pragma once

#include <hgl/graph/IDDHandle.h>
#include <hgl/mtl/InstanceDataLayout.h>
#include <ankerl/unordered_dense.h>
#include <vector>
#include <cstdint>

namespace hgl::graph {

class InstanceDataDomain;
class DomainMaterialBinding;
class BufferManager;
class MITBuffer;

/**
 * IDDManager — InstanceDataDomain 生命周期管理器
 *
 * 持有并管理所有 InstanceDataDomain 实例，以强类型 IDDHandle（id + generation）
 * 对外提供域引用，彻底替代裸指针跨模块传递。
 *
 * 职责：
 *   - 唯一拥有 InstanceDataDomain 的构造权（friend class IDDManager）
 *   - 维护句柄表：domain_table_（id → entry）和 domain_id_map_（ptr → id）
 *   - 提供 MI 槽位管理的统一入口（AllocSlot / FreeSlot / GetSlotData / WriteSlotData）
 *   - 懒创建 GPU 缓冲区（EnsureGPUBuffers，由 Collect System 在首帧触发）
 */
class IDDManager
{
    struct DomainEntry
    {
        InstanceDataDomain *    domain     = nullptr;
        MITBuffer          *    mit_buffer = nullptr;
        uint32_t                generation = 0;  // 0 = invalid/released
    };

    std::vector<DomainEntry>                                         domain_table_;   // index 0 = invalid sentinel
    ankerl::unordered_dense::map<InstanceDataDomain *, uint32_t>     domain_id_map_;  // reverse: ptr → id

    /// 首次注册返回新 id（1-based）；重复注册返回已有 id。
    uint32_t RegisterDomain  (InstanceDataDomain *domain);
    /// 标记条目失效（generation 保留，domain 置 null）；不 delete domain。
    void     UnregisterDomain(InstanceDataDomain *domain);

public:

    IDDManager();
    ~IDDManager();

    // ----------------------------------------------------------------
    // 域生命周期 -- Create / Get / Release
    // ----------------------------------------------------------------

    /**
     * 构造并注册一个新域，返回强类型句柄。
     * 调用方不需要也不应该持有 InstanceDataDomain*；通过 Get(handle) 解引用。
     */
    IDDHandle Create(mtl::InstanceDataLayout layout,uint32_t max_count,uint8_t tex_array_slots = 0);

    /**
     * 通过句柄安全解引用。generation 不匹配或越界时返回 nullptr。
     */
    InstanceDataDomain *Get(IDDHandle handle) const;

    /**
     * 释放域：UnregisterDomain + delete。
     * 调用前请确保无存活 MI 实例引用该域。
     */
    void Release(IDDHandle handle);

    /**
     * 过渡工具：裸指针反查句柄（P4-P6 迁移期使用；P8 后可移除）。
     * 若 domain 未在本 manager 中注册，返回 InvalidIDDHandle。
     */
    IDDHandle GetHandle(InstanceDataDomain *domain) const;

    // ----------------------------------------------------------------
    // MI 槽位管理（P7 完整实现）
    // ----------------------------------------------------------------

    /**
     * 在指定域中分配一个 MI 槽位，可选初始化数据。
     * @return 槽位 id（>= 0），失败返回 -1。
     */
    int    AllocSlot  (IDDHandle handle, const void *init_data = nullptr, uint32_t size = 0);

    /**
     * 释放槽位（不销毁域）。
     */
    void   FreeSlot   (IDDHandle handle, int slot_id);

    /**
     * 返回指定槽位的 CPU 端数据指针；句柄或 slot_id 无效时返回 nullptr。
     */
    void  *GetSlotData    (IDDHandle handle, int slot_id) const;

    /**
     * 向指定槽位写入数据；返回 false 表示参数无效。
     */
    bool   WriteSlotData  (IDDHandle handle, int slot_id, const void *data, uint32_t size);

    // ----------------------------------------------------------------
    // GPU 缓冲区懒初始化（由 RenderPrimitiveCollectSystem 在首帧调用）
    // ----------------------------------------------------------------

    bool EnsureGPUBuffers(IDDHandle handle, BufferManager *bm);

    // ----------------------------------------------------------------
    // MIT (Material Instance Texture ID) buffer 访问
    // ----------------------------------------------------------------

    /**
     * 获取指定域关联的 MITBuffer；句柄无效时返回 nullptr。
     */
    MITBuffer *GetMITBuffer(IDDHandle handle) const;

    // ----------------------------------------------------------------
    // 诊断
    // ----------------------------------------------------------------

    /// 返回当前已注册的有效域数量（不含 sentinel 和已释放条目）。
    uint32_t GetDomainCount() const;
};

} // namespace hgl::graph
