#include <hgl/ecs/support/pipeline/TemplateFactCache.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/ssbo/ModelTemplateRows.h>

namespace hgl::ecs
{
    void TemplateFactCache::Refresh(const PrimitiveTopologyLedger &ledger)
    {
        const uint64_t topo_rev = ledger.GetTopologyRevision();
        const uint64_t pool_rev = ledger.GetStatPoolContentChanges();

        if (valid
         && topo_rev == topology_rev
         && pool_rev == pool_content_rev)
            return;

        facts.Clear();
        valid            = true;
        topology_rev     = topo_rev;
        pool_content_rev = pool_rev;
        ++builds;
    }

    const TemplateFactCache::Facts *
    TemplateFactCache::Resolve(graph::GlobalSSBOBufferRegistry *pool,
                               const uint32_t template_id,
                               Result &result)
    {
        result = Result::Ok;

        if (!pool || template_id == graph::ssbo::kInvalidModelTemplateID)
        {
            result = Result::RowMissing;
            return nullptr;
        }

        if (Facts *cached = facts.GetValuePointer(template_id))
        {
            ++hits;
            return cached;
        }

        ++misses;

        const graph::ssbo::ModelTemplateRow *tpl = pool->GetTypedRow<graph::ssbo::ModelTemplateRow>(
            graph::GlobalSSBOType::ModelTemplatePool, template_id);

        if (!tpl || tpl->part_offset == 0u || tpl->part_count == 0u
                || tpl->part_bucket_count == 0u)
        {
            // 无效模板行**不入缓存**：行被写成有效后下一帧自然会重建事实
            result = Result::RowMissing;
            return nullptr;
        }

        Facts built;
        built.part_offset       = tpl->part_offset;
        built.part_count        = tpl->part_count;
        built.part_bucket_count = tpl->part_bucket_count;
        built.shading_group_count = (tpl->shading_group_count != 0u) ? tpl->shading_group_count : 1u;
        built.slot_count.Resize(static_cast<int>(built.part_bucket_count));
        built.slot_geometry.Resize(static_cast<int>(built.part_bucket_count));

        for (uint32_t k = 0; k < built.part_count; ++k)
        {
            const graph::ssbo::ModelPartRow *part = pool->GetTypedRow<graph::ssbo::ModelPartRow>(
                graph::GlobalSSBOType::ModelPartPool, built.part_offset + k);

            if (!part)
            {
                // 半成品事实绝不出厂：越界模板每次都按原路径拒批
                result = Result::PartMissing;
                return nullptr;
            }

            if (part->geometry_id == 0u || part->bucket_slot >= built.part_bucket_count)
                continue;

            ++built.slot_count[part->bucket_slot];
            built.slot_geometry[part->bucket_slot] = part->geometry_id;
        }

        Facts &slot = facts[template_id];
        slot = std::move(built);

        return &slot;
    }
}