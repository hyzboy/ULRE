#include<hgl/ecs/core/RenderItem.h>

namespace hgl::ecs
{
    // B3-iii：排序键 = **行派生 POD 键**（同键 ⇒ 同簇 ⇒ 实例化合并）。
    // 旧实现按 `*GeometryDataBuffer <=>`（geometry_id / vdm / VAB 列表 / ibo）+ `*GeometryDrawRange <=>`
    // 逐字段内容比较 —— 那两份每实体 CPU 副本已删除：行就是它们的真相，且同 asset 必然同键。
    int RenderItem::Compare(const RenderItem& other) const
    {
        const MeshDrawKey &a = draw_key;
        const MeshDrawKey &b = other.draw_key;

        if (a.geometry_id  != b.geometry_id)  return a.geometry_id  < b.geometry_id  ? -1 : 1;
        if (a.vertex_base  != b.vertex_base)  return a.vertex_base  < b.vertex_base  ? -1 : 1;
        if (a.index_base   != b.index_base)   return a.index_base   < b.index_base   ? -1 : 1;
        if (a.vertex_count != b.vertex_count) return a.vertex_count < b.vertex_count ? -1 : 1;
        if (a.index_count  != b.index_count)  return a.index_count  < b.index_count  ? -1 : 1;
        if (a.is_indexed   != b.is_indexed)   return a.is_indexed   < b.is_indexed   ? -1 : 1;

        // 同键：按 render_item_handle 升序，最大化连号折叠机会
        const auto h1 = GetRenderItemHandle();
        const auto h2 = other.GetRenderItemHandle();
        if (h1 != graph::INVALID_RENDER_ITEM_HANDLE &&
            h2 != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            if (h1 < h2) return -1;
            if (h1 > h2) return 1;
        }

        return 0;
    }
}//namespace hgl::ecs
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/mesh/GeometryDrawRange.h>

namespace hgl::ecs
{
}//namespace hgl::ecs
