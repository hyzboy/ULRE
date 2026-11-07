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
    uint32_t bucket_slot_count = 0; ///< 模板内已分配的素材桶槽数（去重计数）

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

            // 部件 → 素材桶槽号：模板内按 geometry_id **首次出现**去重编号。
            // 该槽号是模板内常量（与批 / pass / 帧无关），展开侧最终桶号 =
            // 批装配期分配的「模板素材桶组基址」+ 本槽号。
            row.bucket_slot = ssbo::kModelPartBucketSlotNone;

            if (row.geometry_id != 0u)
            {
                const ModelPartRow *prev_rows = rows.GetData();

                for (int i = 0; i < rows.GetCount(); ++i)
                {
                    if (prev_rows[i].geometry_id == row.geometry_id)
                    {
                        row.bucket_slot = prev_rows[i].bucket_slot;
                        break;
                    }
                }

                if (row.bucket_slot == ssbo::kModelPartBucketSlotNone)
                    row.bucket_slot = bucket_slot_count++;
            }

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

    // 5) 写模板行。LOD / 阴影代理字段留 0（= 无代理，恒用本体）；
    //    代理由 `SetModelTemplateProxies` 事后回填（T4.1 / T4.2 的唯一写入口径）。
    ModelTemplateRow trow;
    trow.part_offset = part_first;
    trow.part_count  = part_count;
    trow.part_bucket_count = bucket_slot_count;

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

    GLogInfo(u8"[ModelTemplateManager] 模板注册 id=%u parts=%u 素材桶=%u (几何就绪 %u) 区间=[%u,%u]",
             template_id, part_count, bucket_slot_count, ready_geometry,
             part_first, part_first + part_count - 1u);

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

bool ModelTemplateManager::SetModelTemplateProxies(GlobalSSBOBufferRegistry *registry,
                                                   const uint32_t template_id,
                                                   const ProxyConfig &cfg)
{
    using ssbo::ModelTemplateRow;
    using ssbo::ModelPartRow;
    using ssbo::kInvalidModelTemplateID;

    if (!registry || template_id == kInvalidModelTemplateID)
    {
        GLogWarning(u8"[ModelTemplateManager] 代理回填被拒：registry/模板 id 非法（id=%u）", template_id);
        return false;
    }

    const ModelTemplateRow *base_row =
        registry->GetTypedRow<ModelTemplateRow>(GlobalSSBOType::ModelTemplatePool, template_id);

    if (!base_row || base_row->part_count == 0u || base_row->part_offset == 0u)
    {
        GLogWarning(u8"[ModelTemplateManager] 代理回填被拒：模板行无效或为空壳 id=%u", template_id);
        return false;
    }

    // 「有效模板」= 行存在且部件段非空。代理不要求几何就绪（可先占位），
    // 但**必须**是一个真模板 —— 否则展开侧换级换来一个空段，物体整个消失。
    const auto is_valid_template = [registry, template_id](const uint32_t id) -> bool
    {
        if (id == kInvalidModelTemplateID || id == template_id)
            return false;

        const ModelTemplateRow *row =
            registry->GetTypedRow<ModelTemplateRow>(GlobalSSBOType::ModelTemplatePool, id);

        return row && row->part_count != 0u && row->part_offset != 0u;
    };

    ProxyConfig normalized = cfg;

    // ── LOD 级 1..3：id 与阈值成对校验 ──────────────────────────────────
    float prev_ratio = 1.0f;   ///< 阈值必须单调不增（第一个的上界 = 1.0 = 屏幕铺满）

    for (uint32_t i = 0; i < ssbo::kModelLodProxyCount; ++i)
    {
        const uint32_t id    = normalized.lod_proxy_template_ids[i];
        const float    ratio = normalized.lod_screen_ratios[i];

        if (id == 0u)
        {
            normalized.lod_screen_ratios[i] = 0.0f;   // id 为 0 ⇒ 阈值无意义，清 0 保持行内自洽
            continue;
        }

        if (!is_valid_template(id))
        {
            GLogWarning(u8"[ModelTemplateManager] 代理回填被拒：LOD%u 代理 id=%u 不是有效模板（本体 id=%u）",
                        i + 1u, id, template_id);
            return false;
        }

        if (!(ratio > 0.0f) || ratio > 1.0f)
        {
            GLogWarning(u8"[ModelTemplateManager] 代理回填被拒：LOD%u 阈值 %f 不在 (0,1]",
                        i + 1u, static_cast<double>(ratio));
            return false;
        }

        if (ratio > prev_ratio)
        {
            GLogWarning(u8"[ModelTemplateManager] 代理回填被拒：LOD%u 阈值 %f 大于上一级 %f（必须单调不增）",
                        i + 1u, static_cast<double>(ratio), static_cast<double>(prev_ratio));
            return false;
        }

        // 同一代理不得占两个级槽（否则低级的判定永远先命中，高级是死数据）
        for (uint32_t j = 0; j < i; ++j)
        {
            if (normalized.lod_proxy_template_ids[j] == id)
            {
                GLogWarning(u8"[ModelTemplateManager] 代理回填被拒：LOD%u 与 LOD%u 共用代理 id=%u",
                            j + 1u, i + 1u, id);
                return false;
            }
        }

        prev_ratio = ratio;
    }

    // ── 阴影代理（T4.2）：独立于 LOD 级，可以是某一级模板 ────────────────
    if (normalized.shadow_proxy_template_id != 0u
        && !is_valid_template(normalized.shadow_proxy_template_id))
    {
        GLogWarning(u8"[ModelTemplateManager] 代理回填被拒：阴影代理 id=%u 不是有效模板（本体 id=%u）",
                    normalized.shadow_proxy_template_id, template_id);
        return false;
    }

    // ── 校验全过 ⇒ 复制一份行改代理字段再整行写回（行内其它字段逐字节不变）──
    ModelTemplateRow trow = *base_row;

    bool any_lod = false;

    for (uint32_t i = 0; i < ssbo::kModelLodProxyCount; ++i)
    {
        trow.lod_proxy_template_ids[i] = normalized.lod_proxy_template_ids[i];
        trow.lod_screen_ratios[i]      = normalized.lod_screen_ratios[i];
        any_lod = any_lod || normalized.lod_proxy_template_ids[i] != 0u;
    }

    trow.shadow_proxy_template_id = normalized.shadow_proxy_template_id;

    // 位维护：有则置、无则清（幂等 —— 反复回填同一份配置不会累积状态）
    trow.flags = any_lod ? (trow.flags | ssbo::kModelTemplateFlagLodEnabled)
                         : (trow.flags & ~ssbo::kModelTemplateFlagLodEnabled);
    trow.flags = (normalized.shadow_proxy_template_id != 0u)
                     ? (trow.flags | ssbo::kModelTemplateFlagShadowProxy)
                     : (trow.flags & ~ssbo::kModelTemplateFlagShadowProxy);

    // ── 素材桶槽号统一编号（**换级能否落对桶的前提**）────────────────────
    //
    // 展开侧的桶号 = `bucket_base[实体] + part.bucket_slot`，而 `bucket_base` 是 CPU 装批时
    // 按**本体** `part_bucket_count` 给 (批, 模板) 分配的那一段连续桶 —— 组内第 i 个桶的
    // 几何来自本体（+ 代理）的**统一去重序**。因此本体与所有代理的 `bucket_slot` 必须共用
    // 同一套编号，否则代理部件的槽号会指到别的几何的桶里（画错几何，且不越界 = 不报警）。
    //
    // 编号规则：先本体部件序、再各级代理部件序，按 `geometry_id` 首次出现编号；
    // 结果回写进**每个**参与模板的部件行，并把它们的 `part_bucket_count` 都改成并集大小。
    // 无代理时本段与打包时的编号**等价**（本体独占 ⇒ 序完全一致），因此是无代理路径的恒等变换。
    //
    // ⚠ 硬约束（不做运行期校验，靠导入约定保证）：一个代理模板只归属**一个**本体。
    //   两个本体共用同一代理 ⇒ 后回填者会改写前者的槽号 ⇒ 前者画错。见 ModelTemplateRows.h。

    const uint32_t participant_cap = 1u + ssbo::kModelLodProxyCount + 1u;
    uint32_t participants[participant_cap];
    uint32_t participant_count = 0;

    participants[participant_count++] = template_id;

    for (uint32_t i = 0; i < ssbo::kModelLodProxyCount; ++i)
        if (normalized.lod_proxy_template_ids[i] != 0u)
            participants[participant_count++] = normalized.lod_proxy_template_ids[i];

    if (normalized.shadow_proxy_template_id != 0u)
    {
        bool dup = false;

        for (uint32_t i = 0; i < participant_count; ++i)
            dup = dup || participants[i] == normalized.shadow_proxy_template_id;

        if (!dup)
            participants[participant_count++] = normalized.shadow_proxy_template_id;
    }

    // 并集桶数：无代理 ⇒ 本体既有值（恒等变换）；有代理 ⇒ 由重编号段回填。
    uint32_t final_bucket_count = base_row->part_bucket_count;

    if (participant_count > 1u)
    {
        ValueArray<uint32_t> union_geoms;
        union_geoms.Reserve(64);

        for (uint32_t pi = 0; pi < participant_count; ++pi)
        {
            const ModelTemplateRow *prow =
                registry->GetTypedRow<ModelTemplateRow>(GlobalSSBOType::ModelTemplatePool, participants[pi]);

            const ModelPartRow *parts =
                registry->GetTypedRow<ModelPartRow>(GlobalSSBOType::ModelPartPool, prow->part_offset);

            if (!parts)
            {
                GLogError(u8"[ModelTemplateManager] 代理回填失败：模板 %u 的部件段不可读", participants[pi]);
                return false;
            }

            for (uint32_t k = 0; k < prow->part_count; ++k)
            {
                const uint32_t gid = parts[k].geometry_id;

                if (gid == 0u)
                    continue;

                bool known = false;
                const uint32_t *known_geoms = union_geoms.GetData();

                for (int i = 0; i < union_geoms.GetCount(); ++i)
                    known = known || known_geoms[i] == gid;

                if (!known)
                    union_geoms.Add(gid);
            }
        }

        const uint32_t union_count = static_cast<uint32_t>(union_geoms.GetCount());

        final_bucket_count = union_count;

        // 回写：每个参与模板的部件行按并集序重编号（逐模板一次性写回整段），
        // 随后整行写回模板行（`part_bucket_count` 改并集大小）。
        ValueArray<ModelPartRow> scratch;

        for (uint32_t pi = 0; pi < participant_count; ++pi)
        {
            const ModelTemplateRow *prow =
                registry->GetTypedRow<ModelTemplateRow>(GlobalSSBOType::ModelTemplatePool, participants[pi]);

            const ModelPartRow *parts =
                registry->GetTypedRow<ModelPartRow>(GlobalSSBOType::ModelPartPool, prow->part_offset);

            scratch.Clear();
            scratch.Reserve(static_cast<int>(prow->part_count));

            for (uint32_t k = 0; k < prow->part_count; ++k)
            {
                ModelPartRow r = parts[k];

                r.bucket_slot = ssbo::kModelPartBucketSlotNone;

                if (r.geometry_id != 0u)
                {
                    const uint32_t *known_geoms = union_geoms.GetData();

                    for (int i = 0; i < union_geoms.GetCount(); ++i)
                    {
                        if (known_geoms[i] == r.geometry_id)
                        {
                            r.bucket_slot = static_cast<uint32_t>(i);
                            break;
                        }
                    }
                }

                scratch.Add(r);
            }

            if (!registry->WriteRowRange(GlobalSSBOType::ModelPartPool, prow->part_offset,
                                         scratch.GetData(), prow->part_count))
            {
                GLogError(u8"[ModelTemplateManager] 代理回填失败：模板 %u 部件段重编号写入失败",
                          participants[pi]);
                return false;
            }

            ModelTemplateRow updated = *prow;
            updated.part_bucket_count = union_count;

            if (!registry->Write(GlobalSSBOType::ModelTemplatePool, participants[pi], updated))
            {
                GLogError(u8"[ModelTemplateManager] 代理回填失败：模板 %u 行写入失败", participants[pi]);
                return false;
            }
        }
    }

    trow.part_bucket_count = final_bucket_count;

    if (!registry->Write(GlobalSSBOType::ModelTemplatePool, template_id, trow))
    {
        GLogError(u8"[ModelTemplateManager] 代理回填写入失败：id=%u", template_id);
        return false;
    }

    GLogInfo(u8"[ModelTemplateManager] 代理回填 id=%u LOD=[%u/%u/%u] 阈值=[%.4f/%.4f/%.4f] 阴影=%u",
             template_id,
             trow.lod_proxy_template_ids[0], trow.lod_proxy_template_ids[1],
             trow.lod_proxy_template_ids[2],
             static_cast<double>(trow.lod_screen_ratios[0]),
             static_cast<double>(trow.lod_screen_ratios[1]),
             static_cast<double>(trow.lod_screen_ratios[2]),
             trow.shadow_proxy_template_id);

    return true;
}

} // namespace hgl::graph
