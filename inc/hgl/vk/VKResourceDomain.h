#pragma once

#include <cstdint>
#include <hgl/mtl/ShaderDataSchema.h>

namespace hgl
{
    class ActiveMemoryBlockManager;
}

namespace hgl::graph
{

/**
 * 资源域 (ResourceDomain)
 *
 * 持有一份按 ShaderDataSchema 组织的 MaterialBindingInstance 数据池。
 */
class ResourceDomain
{
    mtl::ShaderDataSchema schema = mtl::ShaderDataSchema::None;
    uint32_t domain_id           = 0;

    uint32_t  mi_data_bytes     = 0;        ///< 单个 MI 数据 stride（由 schema 推导）
    uint32_t  initial_capacity  = 256;      ///< 逻辑初始容量；当前仅用于记录配置

    hgl::ActiveMemoryBlockManager *mi_data_manager = nullptr;  ///< 该域独立的 MI 数据池

private:

    friend class ResourceDomainManager;
    friend class ShaderMaterialProgramManager;
    friend class ShaderMaterialProgram;          ///< Phase 5: ShaderMaterialProgram::CreateMI 需创建默认域

    ResourceDomain(mtl::ShaderDataSchema schema, uint32_t domain_id, uint32_t initial_capacity = 256);

public:

    virtual ~ResourceDomain();

    // ----------------------------------------------------------------
    // 基础属性查询
    // ----------------------------------------------------------------

    bool     hasMI()          const { return mi_data_bytes > 0; }
    uint32_t GetMIDataBytes() const { return mi_data_bytes; }
    uint32_t GetInitialCapacity() const { return initial_capacity; }
    uint32_t GetDomainID() const { return domain_id; }
    mtl::ShaderDataSchema GetShaderDataSchema() const { return schema; }

    // ----------------------------------------------------------------
    // MI 槽位管理 — 仅被 MaterialInstanceData 析构路径和 CreateMI 使用
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

