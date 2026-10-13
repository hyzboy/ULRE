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

}//namespace hgl::ecs
