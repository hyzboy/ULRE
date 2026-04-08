#pragma once

#include <hgl/mtl/InstanceDataLayout.h>

namespace hgl
{
    class ActiveMemoryBlockManager;
}

namespace hgl::graph
{

/**
 * 资源域 (MaterialResourceDomain)
 *
 * 持有与特定 InstanceDataLayout 兼容的独立 MaterialInstance 数据池，同时声明该域
 * 提供的 TextureArray slot 集合（供方）。
 * 同一套 Shader/GraphicsPipeline 可关联多个 MaterialResourceDomain，使不同的资源集合
 * （例如UI图标 vs 角色头像 Billboard）彼此隔离，互不串绑。
 */
class MaterialResourceDomain
{
    mtl::InstanceDataLayout  instance_layout         = mtl::InstanceDataLayout::None;
    uint32_t                 mi_max_count             = 0;       ///< 渲染批次最大实例数
    uint8_t                  texture_array_slot_flags = 0;       ///< 供方：该域提供哪些 TextureArray slot

    hgl::ActiveMemoryBlockManager *mi_data_manager = nullptr;    ///< 该域独立的 MI 数据池

private:

    friend class MaterialManager;

    explicit MaterialResourceDomain(mtl::InstanceDataLayout layout,
                                    uint32_t max_count,
                                    uint8_t tex_array_slots = 0);

public:

    virtual ~MaterialResourceDomain();

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

}; // class MaterialResourceDomain

} // namespace hgl::graph

