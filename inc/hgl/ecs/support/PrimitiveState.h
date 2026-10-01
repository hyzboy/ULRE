#pragma once

#include<hgl/ecs/core/EntityHandle.h>
#include<hgl/graph/render/RenderItemDescriptor.h>
#include<cstdint>

namespace hgl
{
    namespace graph
    {
        class DeviceBuffer;
        class IndirectMeshTaskBuffer;
    }
}

namespace hgl::ecs
{
    class ECSContext;
    class Entity;

    // ─────────────────────────────────────────────────────────────
    // PrimitiveState —— 原图元渲染组件（普通 / 多实例 / 可渲染基类）
    // 删除后（A5b）残留的**实体级**渲染状态入口。
    //
    // 分两类，都不新造组件：
    //   · **能力判定**：纯粹由实体现有组件集合推导（几何 / 材质数据层），做成自由函数；
    //   · **每实例绑定**：与授权态无关、每实体一份的可变绑定（render_item 句柄 / 4-ID、
    //     多实例连续槽位与 GPU 绑定、按 pass 解析出的运行期管线），落在世界
    //     `MaterialRuntimeTable` 的**每实例 slot**（见 support/MaterialRuntimeTable.h）。
    // ─────────────────────────────────────────────────────────────

    // ── 能力判定（实体现有组件集合；不创建任何组件）──

    /// 实体是否具备可渲染资源（现 = 有 GeometryData 且 `GetPrimitiveAsset() != nullptr`）。
    /// A5b：原图元组件 CanRender 的自由函数化（可见性真值在实体级，不在此判据内）。
    bool CanRender(const Entity *entity);

    /// 实体是否有材质来源（材质数据层有配方覆盖，或几何 asset 里有默认配方）。
    bool HasAnyMaterialSource(const Entity *entity);

    // ── render_item 4-ID 绑定（世界 RenderItemDataStorage；句柄存每实体 slot）──

    /// 实体 → render_item 句柄（未分配则按需在世界存储分配并写进 slot）。无世界 ⇒ INVALID。
    graph::RenderItemHandle EnsureRenderItemHandle(ECSContext &world, EntityID owner);

    /// 实体 render_item 句柄（未分配 ⇒ INVALID；**不**分配）
    graph::RenderItemHandle GetRenderItemHandle(const ECSContext &world, EntityID owner);

    /// 实体 → 写 4-ID 到世界渲染项存储（按需分配句柄；单槽语义）。
    bool SetRenderItem4ID(ECSContext &world, EntityID owner,
                          uint32_t transform_id, uint32_t geometry_id,
                          uint32_t material_id, uint32_t texture_id);

    /// 释放实体 render_item 句柄（连续区间一并释放；实体销毁/世界关停时调用）。
    void ReleaseRenderItemHandle(ECSContext &world, EntityID owner);

    // ── 多实例连续槽位（原图元多实例组件状态）──

    bool     AllocateContiguousInstances(ECSContext &world, EntityID owner, uint32_t count);
    void     ReleaseInstances(ECSContext &world, EntityID owner);
    uint32_t GetAllocatedInstanceCapacity(const ECSContext &world, EntityID owner);
    uint32_t GetInstanceCount(const ECSContext &world, EntityID owner);
    void     SetInstanceCount(ECSContext &world, EntityID owner, uint32_t count);
    void     SetMaxInstances(ECSContext &world, EntityID owner, uint32_t max_count);

    graph::RenderItemHandle GetInstanceHandle(const ECSContext &world, EntityID owner, uint32_t instance_idx);
    bool SetInstance4ID(ECSContext &world, EntityID owner, uint32_t instance_idx,
                        uint32_t transform_id, uint32_t geometry_id, uint32_t material_id, uint32_t texture_id);
    bool SetAllInstances4ID(ECSContext &world, EntityID owner,
                            uint32_t base_transform_id, uint32_t geometry_id,
                            uint32_t material_id, uint32_t texture_id,
                            bool sequential_transforms = true);

    // ── 多实例 GPU 绑定（BDA 缓冲；每实体一份）──
    void SetL2WBuffer(ECSContext &world, EntityID owner, graph::DeviceBuffer *buf);
    void SetL2WIndexBuffer(ECSContext &world, EntityID owner, graph::DeviceBuffer *buf);
    void SetMeshDrawParamsBuffer(ECSContext &world, EntityID owner, graph::DeviceBuffer *buf);
    void SetMaterialDataRowsBuffer(ECSContext &world, EntityID owner, graph::DeviceBuffer *buf);
    void SetIndirectMeshTaskBuffer(ECSContext &world, EntityID owner, graph::IndirectMeshTaskBuffer *buf);
    void SetIndirectCountBuffer(ECSContext &world, EntityID owner, graph::DeviceBuffer *buf, uint64_t offset = 0);
    void SetGPUDriven(ECSContext &world, EntityID owner, bool enabled);
}//namespace hgl::ecs
