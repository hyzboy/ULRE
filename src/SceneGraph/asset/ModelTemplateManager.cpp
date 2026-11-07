// ModelTemplateManager —— 模型内容 → GPU 常驻模板池的打包（plan T1.3）。
//
// 只写 GlobalSSBOBufferRegistry 的两个行池（ModelTemplatePool / ModelPartPool），
// 不持有任何 CPU 侧缓存；约定与口径全部见 ModelTemplateManager.h / ModelTemplateRows.h。

#include <hgl/graph/asset/ModelTemplateManager.h>

#include <hgl/log/Log.h>
#include <hgl/type/ValueArray.h>
#include <hgl/math/geometry/AABB.h>
#include <hgl/graph/asset/ModelAsset.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/graph/mesh/StaticMesh.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/ssbo/ModelTemplateRows.h>

namespace hgl::graph
{

uint32_t ModelTemplateManager::RegisterModelTemplate(GlobalSSBOBufferRegistry *registry,
                                                     const ModelAssetContent &content)
{
    using ssbo::ModelTemplateRow;
    using ssbo::ModelPartRow;
    using ssbo::kInvalidModelTemplateID;

    if (!registry)
    {
        GLogError(u8"[ModelTemplateManager] 注册失败：registry 为空");
        return kInvalidModelTemplateID;
    }

    const size_t node_count = content.nodes.size();
    const size_t asset_count = content.assets.size();

    // 1) 先数部件：(节点 × primitive) 的每个可绘制叶子。越界引用防御性跳过并留痕。
    uint32_t part_count = 0;

    for (size_t n = 0; n < node_count; ++n)
    {
        const StaticMeshNode &node = content.nodes[n];

        for (int32_t pi : node.primitiveIndices)
        {
            if (pi >= 0 && static_cast<size_t>(pi) < asset_count)
                ++part_count;
        }
    }

    if (part_count == 0)
    {
        GLogWarning(u8"[ModelTemplateManager] 模型无可打包部件（nodes=%zu assets=%zu），跳过注册",
                    node_count, asset_count);
        return kInvalidModelTemplateID;
    }

    // 2) 部件连续区间 + 模板行各发一次号。任何一步失败都把已发的号全部还回去
    //    （全有或全无：不留半截区间在池里）。
    const uint32_t template_id = registry->Acquire(GlobalSSBOType::ModelTemplatePool);

    if (template_id == ActiveRowPool::InvalidRowID)
    {
        GLogError(u8"[ModelTemplateManager] 模板行发号失败：池容量 %u 已满",
                  registry->GetCapacity(GlobalSSBOType::ModelTemplatePool));
        return kInvalidModelTemplateID;
    }

    const uint32_t part_first = registry->AcquireRowRange(GlobalSSBOType::ModelPartPool, part_count);

    if (part_first == ActiveRowPool::InvalidRowID)
    {
        GLogError(u8"[ModelTemplateManager] 部件区间发号失败：需要 %u 行 / 池容量 %u",
                  part_count, registry->GetCapacity(GlobalSSBOType::ModelPartPool));
        registry->ReleaseID(GlobalSSBOType::ModelTemplatePool, template_id);
        return kInvalidModelTemplateID;
    }

    // 3) CPU 侧拼部件行。包围盒缺失的部件用「负半径 = 始终可见」哨兵
    //    （与 GeometryAABB 行 0 同一安全侧哲学：数据缺失只能多画，不能消失）。
    ValueArray<ModelPartRow> rows;
    rows.Reserve(static_cast<int>(part_count));

    hgl::math::AABB merged_box;
    merged_box.Clear();
    bool any_bounds = false;
    uint32_t ready_geometry = 0;

    for (size_t n = 0; n < node_count; ++n)
    {
        const StaticMeshNode &node = content.nodes[n];

        // 部件包围盒：节点包围盒在**节点自身局部空间**（与 local_matrix 同坐标系，
        // 展开侧再用 实例L2W × local_matrix 变换测试）。
        const bool bounds_ok = node.boundsValid && !node.nodeBounds.aabb.IsEmpty();
        const hgl::math::Vector3f part_center = bounds_ok ? node.nodeBounds.aabb.GetCenter()
                                                          : hgl::math::Vector3f(0.0f, 0.0f, 0.0f);
        const hgl::math::Vector3f part_extent = bounds_ok ? node.nodeBounds.aabb.GetExtent()
                                                          : hgl::math::Vector3f(0.0f, 0.0f, 0.0f);
        const float part_radius = bounds_ok ? node.nodeBounds.aabb.GetDiagonal() * 0.5f
                                            : GlobalSSBOBufferRegistry::kAlwaysVisibleRadius;

        // 模板包围盒：节点世界（=模型根空间）包围盒合并 —— 与
        // StaticMeshModelInstance::InstantiateSkeleton 的同款口径。
        if (bounds_ok)
        {
            merged_box.Merge(node.nodeBounds.aabb.Transformed(node.worldMatrix));
            any_bounds = true;
        }

        for (int32_t pi : node.primitiveIndices)
        {
            if (pi < 0 || static_cast<size_t>(pi) >= asset_count)
                continue;

            const PrimitiveAsset &asset = content.assets[static_cast<size_t>(pi)];
            Geometry *geometry = asset.GetGeometry();

            ModelPartRow row;
            row.local_matrix = node.worldMatrix;
            row.aabb_center_radius = hgl::math::Vector4f(part_center.x, part_center.y, part_center.z,
                                                         part_radius);
            row.aabb_extents = hgl::math::Vector4f(part_extent.x, part_extent.y, part_extent.z, 0.0f);

            // 几何未流送就绪时 GetGeometryID() 为 0 ⇒ 展开跳过该部件（降级不多不少）。
            row.geometry_id = geometry ? geometry->GetGeometryID() : 0u;
            if (row.geometry_id != 0u)
                ++ready_geometry;

            // material_id：模板层不定死材质（批次/实例材质在 collect 侧回退），先行占 0。
            row.material_id = 0u;
            row.part_flags  = row.geometry_id != 0u ? ssbo::kModelPartFlagCanCastShadow : 0u;

            rows.Add(row);
        }
    }

    // 行数必须与发号口径一致（两处计数共用同一循环条件，走形 = 写穿相邻模板的区间）。
    if (static_cast<uint32_t>(rows.GetCount()) != part_count)
    {
        GLogError(u8"[ModelTemplateManager] 部件行数与发号不符：rows=%d expect=%u",
                  rows.GetCount(), part_count);
        registry->ReleaseRowRange(GlobalSSBOType::ModelPartPool, part_first, part_count);
        registry->ReleaseID(GlobalSSBOType::ModelTemplatePool, template_id);
        return kInvalidModelTemplateID;
    }

    // 4) 写部件区间（行宽在 WriteRowRange 内校验 = 128B）。
    if (!registry->WriteRowRange(GlobalSSBOType::ModelPartPool, part_first,
                                 rows.GetData(), part_count))
    {
        GLogError(u8"[ModelTemplateManager] 部件区间写入失败：first=%u count=%u",
                  part_first, part_count);
        registry->ReleaseRowRange(GlobalSSBOType::ModelPartPool, part_first, part_count);
        registry->ReleaseID(GlobalSSBOType::ModelTemplatePool, template_id);
        return kInvalidModelTemplateID;
    }

    // 5) 写模板行。LOD / 阴影代理字段留 0（T4 空壳：由代理登记接口后续回填）。
    ModelTemplateRow trow;
    trow.part_offset = part_first;
    trow.part_count  = part_count;

    if (any_bounds)
    {
        const hgl::math::Vector3f c = merged_box.GetCenter();
        const hgl::math::Vector3f e = merged_box.GetExtent();

        trow.local_bound_center_radius = hgl::math::Vector4f(c.x, c.y, c.z,
                                                             merged_box.GetDiagonal() * 0.5f);
        trow.local_bound_extents = hgl::math::Vector4f(e.x, e.y, e.z, 0.0f);
    }
    else
    {
        trow.local_bound_center_radius = hgl::math::Vector4f(0.0f, 0.0f, 0.0f,
                                                             GlobalSSBOBufferRegistry::kAlwaysVisibleRadius);
    }

    if (!registry->Write(GlobalSSBOType::ModelTemplatePool, template_id, trow))
    {
        GLogError(u8"[ModelTemplateManager] 模板行写入失败：id=%u", template_id);
        registry->ReleaseRowRange(GlobalSSBOType::ModelPartPool, part_first, part_count);
        registry->ReleaseID(GlobalSSBOType::ModelTemplatePool, template_id);
        return kInvalidModelTemplateID;
    }

    GLogInfo(u8"[ModelTemplateManager] 模板注册 id=%u parts=%u (几何就绪 %u) 区间=[%u,%u]",
             template_id, part_count, ready_geometry, part_first, part_first + part_count - 1u);

    return template_id;
}

bool ModelTemplateManager::UnregisterModelTemplate(GlobalSSBOBufferRegistry *registry,
                                                   const uint32_t template_id)
{
    if (!registry || template_id == ssbo::kInvalidModelTemplateID)
        return false;

    const ssbo::ModelTemplateRow *row = registry->GetTypedRow<ssbo::ModelTemplateRow>(
        GlobalSSBOType::ModelTemplatePool, template_id);

    if (!row || row->part_count == 0u || row->part_offset == 0u)
    {
        GLogWarning(u8"[ModelTemplateManager] 注销被拒：模板行无效或为空壳 id=%u", template_id);
        return false;
    }

    const bool parts_ok = registry->ReleaseRowRange(GlobalSSBOType::ModelPartPool,
                                                    row->part_offset, row->part_count);
    const bool self_ok  = registry->ReleaseID(GlobalSSBOType::ModelTemplatePool, template_id);

    if (parts_ok && self_ok)
        GLogInfo(u8"[ModelTemplateManager] 模板注销 id=%u 区间归还 [%u,%u]",
                 template_id, row->part_offset, row->part_offset + row->part_count - 1u);

    return parts_ok && self_ok;
}

} // namespace hgl::graph
