#include <hgl/ecs/support/pipeline/MaterialBatchPlanCache.h>
#include <hgl/ecs/core/render/RenderFrameCache.h>
#include <hgl/ecs/core/render/MaterialBatch.h>
#include <hgl/ecs/core/render/RenderItem.h>
#include <hgl/ecs/support/transform/TransformAssignmentBuffer.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/util/hash/FNV1a.h>
#include <hgl/ecs/support/storage/FrameProfiler.h>

namespace hgl::ecs
{
    void MaterialBatchPlanCache::Reset()
    {
        for (Slot &slot : plans)
        {
            slot.valid = false;
            slot.batches.Clear();
            slot.item_batch.Clear();
            slot.visible = 0;
            slot.item_sig = 0;
        }
    }

    MaterialBatchPlanCache::Slot *MaterialBatchPlanCache::Find(const void *render_pass)
    {
        for (Slot &slot : plans)
        {
            if (slot.render_pass == render_pass)
                return &slot;
        }

        return nullptr;
    }

    MaterialBatchPlanCache::Slot *MaterialBatchPlanCache::Acquire(const void *render_pass)
    {
        if (Slot *slot = Find(render_pass))
            return slot;

        // 现实中只有 Main / Shadow 两槽；超过上限说明 pass 身份不稳定（例如每帧新建
        // 离屏目标），此时整体作废重来，绝不无限增长。
        if (plans.GetCount() >= 8)
            plans.Clear();

        Slot slot;
        slot.render_pass = render_pass;
        plans.Append(slot);

        return &plans[plans.GetCount() - 1];
    }

    uint64_t MaterialBatchPlanCache::ComputeSignature(const RenderFrameCache &cache,
                                                      uint32_t &visible_count) const
    {
        uint64_t hash = hgl::hash::FNV1aInit<uint64_t>();

        visible_count = 0;

        for (const auto &itemPtr : cache.renderItems)
        {
            const RenderItem *item = itemPtr.get();

            if (!item)
                continue;

            const EntityID id = item->GetEntityID();

            hash = hgl::hash::FNV1aAppendBytes(hash,
                                               reinterpret_cast<const uint8_t *>(&id.index),
                                               sizeof(id.index));
            hash = hgl::hash::FNV1aAppendBytes(hash,
                                               reinterpret_cast<const uint8_t *>(&id.generation),
                                               sizeof(id.generation));

            const uint8_t visible = item->isVisible ? 1u : 0u;

            hash = hgl::hash::FNV1aAppendBytes(hash, &visible, 1);

            if (item->isVisible)
                ++visible_count;
        }

        return hash;
    }

    bool MaterialBatchPlanCache::TryReplay(RenderFrameCache &cache,
                                           Slot &slot,
                                           const PrimitiveTopologyLedger &ledger,
                                           TransformAssignmentBuffer *shared_transform_buffer,
                                           graph::BufferManager *buffer_manager,
                                           graph::VulkanDevice *device)
    {
        if (!slot.valid || slot.batches.IsEmpty())
        {
            ++misses;
            return false;
        }

        if (ledger.GetTopologyRevision() != slot.topology_rev
         || ledger.GetDynamicFactRevision(DynamicFactChange::MaterialContent) != slot.fact_rev)
        {
            ++misses;
            return false;
        }

        uint32_t visible_count = 0;
        const uint64_t sig = ComputeSignature(cache, visible_count);

        if (sig != slot.item_sig || visible_count != slot.visible)
        {
            ++misses;
            return false;
        }

        // 计划自洽性预校验（缺陷防线，非常态分支）：项→批下标必须全部有效。
        // 任何一条不成立就整份计划作废 —— 绝不在**已部分构建**的批表上留半成品。
        for (const uint32_t bi : slot.item_batch)
        {
            if (bi != kNoPlanBatch && bi >= static_cast<uint32_t>(slot.batches.GetCount()))
            {
                Reset();
                ++misses;
                return false;
            }
        }

        // ── 重放 ────────────────────────────────────────────────────────────────
        ValueArray<MaterialBatch *> created;
        created.Resize(slot.batches.GetCount());

        uint32_t vi = 0;

        for (const auto &itemPtr : cache.renderItems)
        {
            RenderItem *item = itemPtr.get();

            if (!item || !item->isVisible)
                continue;

            const uint32_t bi = slot.item_batch[vi];

            if (bi == kNoPlanBatch)
            {
                // 建计划那一帧就没进任何批（管线/配方行未解析）⇒ 本帧同样跳过，
                // 与慢路径逐位一致。
                ++vi;
                continue;
            }

            MaterialBatch *batch = created[bi];

            if (!batch)
            {
                const ShaderProgramPipelineKey &key = slot.batches[bi].key;

                auto batch_owner = std::unique_ptr<MaterialBatch>(new MaterialBatch(key, device, buffer_manager));
                batch_owner->transform_buffer = shared_transform_buffer;
                batch = batch_owner.get();

                cache.materialBatches[key] = std::move(batch_owner);
                created[bi] = batch;
            }

            batch->AddItem(item);
            ++vi;
        }

        ++hits;

        FrameProfiler::Count(FrameProfileCount::Batches, cache.materialBatches.GetCount());

        return true;
    }

    void MaterialBatchPlanCache::Store(Slot &slot,
                                       uint64_t topology_rev,
                                       uint64_t fact_rev,
                                       uint64_t item_sig,
                                       uint32_t visible,
                                       ValueArray<CachedBatchPlan> &&batches,
                                       ValueArray<uint32_t> &&item_batch)
    {
        slot.topology_rev = topology_rev;
        slot.fact_rev     = fact_rev;
        slot.item_sig     = item_sig;
        slot.visible      = visible;
        slot.batches      = batches;
        slot.item_batch   = item_batch;
        slot.valid        = !slot.batches.IsEmpty();

        ++builds;
    }
}