#pragma once

#include <cstdint>

namespace hgl
{
    class ActiveMemoryBlockManager;
}

namespace hgl::graph
{

class Material;

/**
 * 资源域 (ResourceDomain)
 *
 * 持有与特定 Material 模板兼容的独立 MaterialInstance 数据池。
 * 同一套 Shader/Pipeline 可关联多个 ResourceDomain，使不同的资源集合
 * （例如UI图标 vs 角色头像 Billboard）彼此隔离，互不串绑。
 *
 * Phase 1: 已支持 MI 数据池。Texture/Sampler 绑定在后续阶段引入。
 */
class ResourceDomain
{
    Material *source_material   = nullptr;  ///< 该域基于的 Material 模板

    uint32_t  mi_data_bytes     = 0;        ///< 单个 MI 数据 stride（从 source_material 复制）
    uint32_t  mi_max_count      = 0;        ///< 渲染批次最大实例数（从 source_material 复制）

    hgl::ActiveMemoryBlockManager *mi_data_manager = nullptr;  ///< 该域独立的 MI 数据池

private:

    friend class MaterialManager;
    friend class Material;          ///< Phase 5: Material::CreateMI 需创建默认域

    explicit ResourceDomain(Material *mtl);

public:

    virtual ~ResourceDomain();

    // ----------------------------------------------------------------
    // 基础属性查询
    // ----------------------------------------------------------------

    Material *GetSourceMaterial() const { return source_material; }

    bool     hasMI()          const { return mi_data_bytes > 0; }
    uint32_t GetMIDataBytes() const { return mi_data_bytes; }
    uint32_t GetMIMaxCount()  const { return mi_max_count; }

    // ----------------------------------------------------------------
    // MI 槽位管理 — 仅被 MaterialInstance 析构路径和 CreateMI 使用
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

}; // class ResourceDomain

} // namespace hgl::graph

