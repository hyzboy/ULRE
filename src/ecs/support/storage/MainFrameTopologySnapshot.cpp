#include<hgl/ecs/support/storage/MainFrameTopologySnapshot.h>
#include<hgl/ecs/core/render/RenderFrameCache.h>
#include<hgl/ecs/core/render/MaterialBatch.h>
#include<hgl/ecs/core/render/PrimitiveRenderItem.h>
#include<hgl/util/hash/FNV1a.h>
#include<algorithm>
#include<cstdlib>

namespace hgl::ecs
{
    namespace
    {
        /// 非 `PrimitiveRenderItem` 成员的哨兵（例如未来其它类型的渲染项）。
        /// 显式取 `uint32_t` 全 1，`reserved0` 恒为 0 ⇒ 逐字节对拍稳定。
        constexpr uint32_t kNoEntity     = 0xFFFFFFFFu;
        constexpr uint32_t kNoMaterialRow = 0xFFFFFFFFu;

        inline uint64_t PointerIdentity(const void *p)
        {
            return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
        }
    }//namespace

    const char *GetSnapshotStateName(SnapshotState state)
    {
        switch (state)
        {
            case SnapshotState::Uninitialized: return "uninit";
            case SnapshotState::Ready:         return "ready";
            case SnapshotState::Invalid:       return "invalid";
            default:                           return "<bad>";
        }
    }

    bool GetMainFrameTopologySnapshotEnvDefault()
    {
        // 函数内 static：规避静态初始化顺序问题（与 FrameProfiler 的通道默认值同一手法）。
        static const bool s_enabled = []
        {
            const char *value = getenv("ULRE_MAINFRAME_TOPO_CACHE");

            if (!value || !*value)
                return false;

            return value[0] == '1'
                || value[0] == 't' || value[0] == 'T'
                || value[0] == 'y' || value[0] == 'Y';
        }();

        return s_enabled;
    }

    bool GetMainFrameCollectCacheEnvDefault()
    {
        static const bool s_enabled = []
        {
            const char *value = getenv("ULRE_MAINFRAME_COLLECT_CACHE");

            if (!value || !*value)
                return false;

            return value[0] == '1'
                || value[0] == 't' || value[0] == 'T'
                || value[0] == 'y' || value[0] == 'Y';
        }();

        return s_enabled;
    }

    void MainFrameTopologySnapshot::ClearPending()
    {
        items[pending_slot].Clear();
        batches[pending_slot].Clear();
    }

    void MainFrameTopologySnapshot::ReleaseLive()
    {
        items[live_slot].Clear();
        batches[live_slot].Clear();
    }

    bool MainFrameTopologySnapshot::BuildFromCache(const RenderFrameCache &cache,
                                                  const uint64_t in_topology_revision,
                                                  const uint32_t in_frame_serial)
    {
        // 重入护栏：构建期理论上不会重入（只在批阶段尾部调一次），
        // 但真重入时留旧 live 比留半截 pending 安全。
        if (building)
        {
            ++stats.aborts;
            return false;
        }

        building = true;
        ++stats.builds;

        ClearPending();

        ValueArray<SnapshotBatchRecord> &out_batches = batches[pending_slot];
        ValueArray<SnapshotItemRecord>  &out_items   = items[pending_slot];

        // 规范批序：只取「有成员」的批，并按 `ShaderProgramPipelineKey::operator<`
        // 排序 —— 与 `PrimitiveRenderSystem::RenderBatches` 的批间排序同一依据。
        ValueArray<MaterialBatch *> ordered;
        ordered.Reserve(cache.materialBatches.GetCount());

        for (auto &pair : cache.materialBatches)
        {
            MaterialBatch *batch = pair.second.get();

            // 空批 = 本帧不绘制 ⇒ 不属于拓扑。**不要**在这里顺手把它从 map 里摘掉：
            // P2 对 cache 只读。
            if (!batch || batch->items.empty())
                continue;

            ordered.Add(batch);
        }

        const int batch_count = ordered.GetCount();

        if (batch_count > 1)
            std::sort(ordered.begin(),
                      ordered.end(),
                      [](const MaterialBatch *a, const MaterialBatch *b)
                      {
                          return a->key < b->key;
                      });

        int total_items = 0;
        for (int i = 0; i < batch_count; ++i)
            total_items += static_cast<int>(ordered[i]->items.size());

        out_batches.Reserve(batch_count);
        out_items.Reserve(total_items);

        uint32_t cursor = 0;

        for (int bi = 0; bi < batch_count; ++bi)
        {
            const MaterialBatch *batch = ordered[bi];

            // 指针值只作身份令牌折叠，**不解引用、不保存可转回指针的原始值组合**。
            hgl::hash::FNV1aHasher64 key_hasher;
            key_hasher << PointerIdentity(batch->key.shader_program)
                       << PointerIdentity(batch->key.pipeline);

            SnapshotBatchRecord record;
            record.key_identity = key_hasher.Result();
            record.key_config   = static_cast<uint64_t>(graph::mtl::HashMaterialPipelineConfig(batch->key.pipeline_config));
            record.first_item   = cursor;
            record.item_count   = static_cast<uint32_t>(batch->items.size());

            out_batches.Add(record);

            for (RenderItem *render_item : batch->items)
            {
                SnapshotItemRecord item_record;
                item_record.batch_slot = static_cast<uint32_t>(bi);

                if (const PrimitiveRenderItem *prim = dynamic_cast<const PrimitiveRenderItem *>(render_item))
                {
                    const EntityID entity_id = prim->GetEntityID();

                    item_record.entity_index      = entity_id.index;
                    item_record.entity_generation = entity_id.generation;
                    item_record.material_row      = prim->GetMaterialRuntimeRowID();

                    // 绘制键：完整路径刚刚在 `PublishPrimitiveFacts` 里算好 ⇒ 搬进快照是
                    // **零额外成本**，却让快路径（P3）无需再读映射 GPU 缓冲里的 params 行。
                    const RenderItem::MeshDrawKey &key = prim->draw_key;

                    item_record.draw_geometry_id  = key.geometry_id;
                    item_record.draw_vertex_base  = key.vertex_base;
                    item_record.draw_index_base   = key.index_base;
                    item_record.draw_vertex_count = key.vertex_count;
                    item_record.draw_index_count  = key.index_count;
                    item_record.draw_is_indexed   = key.is_indexed;
                }
                else
                {
                    item_record.entity_index      = kNoEntity;
                    item_record.entity_generation = 0;
                    item_record.material_row      = kNoMaterialRow;
                }

                out_items.Add(item_record);
                ++cursor;
            }
        }

        // 内容哈希：覆盖计数与全部记录字段（不按原始字节折，避免结构体填充位干扰）。
        hgl::hash::FNV1aHasher64 hasher;
        hasher << static_cast<uint64_t>(out_batches.GetCount())
               << static_cast<uint64_t>(out_items.GetCount());

        const SnapshotBatchRecord *batch_data = out_batches.GetData();
        for (int i = 0; i < out_batches.GetCount(); ++i)
        {
            hasher << batch_data[i].key_identity
                   << batch_data[i].key_config
                   << batch_data[i].first_item
                   << batch_data[i].item_count;
        }

        const SnapshotItemRecord *item_data = out_items.GetData();
        for (int i = 0; i < out_items.GetCount(); ++i)
        {
            hasher << item_data[i].entity_index
                   << item_data[i].entity_generation
                   << item_data[i].material_row
                   << item_data[i].batch_slot
                   << item_data[i].draw_geometry_id
                   << item_data[i].draw_vertex_base
                   << item_data[i].draw_index_base
                   << item_data[i].draw_vertex_count
                   << item_data[i].draw_index_count
                   << item_data[i].draw_is_indexed;
        }

        const uint64_t new_hash = hasher.Result();

        // 对拍：只有「上一份 live 就绪」才有比较基线。
        // `parity_mismatch` = 台帐说拓扑没变、内容却变了 ⇒ 存在**未上报**的写者，
        // 这是 P2 唯一真正的告警项（P3 一旦据此跳过，就是静默丢绘制）。
        if (state != SnapshotState::Ready)
            ++stats.misses;
        else if (topology_revision != in_topology_revision)
            ++stats.misses;
        else if (content_hash != new_hash)
        {
            ++stats.parity_mismatch;
            ++stats.misses;
        }
        else
            ++stats.hits;

        // 原子提交：只翻转槽下标，不搬移任何容器。
        const uint32_t previous_live = live_slot;
        live_slot    = pending_slot;
        pending_slot = previous_live;

        topology_revision = in_topology_revision;
        content_hash      = new_hash;
        frame_serial      = in_frame_serial;
        collected_items   = static_cast<uint32_t>(cache.renderItems.size());

        state = SnapshotState::Ready;

        stats.last_items   = static_cast<uint64_t>(out_items.GetCount());
        stats.last_batches = static_cast<uint64_t>(out_batches.GetCount());

        if (out_items.GetCount() == 0)
            ++stats.empty_builds;

        ++stats.commits;

        building = false;
        return true;
    }

    void MainFrameTopologySnapshot::Invalidate()
    {
        // 保留已提交内容与哈希：报表仍能显示「最后一份快照长什么样」。
        // 状态转为 Invalid ⇒ 下一次构建的 `state != Ready` 分支记 miss，
        // 相当于丢掉比较基线（这份快照本来就已经不可复用了）。
        ++stats.invalidations;
        state = SnapshotState::Invalid;
    }

    void MainFrameTopologySnapshot::ResetStats()
    {
        stats = SnapshotStats{};
    }

    void MainFrameTopologySnapshot::Reset()
    {
        ReleaseLive();
        ClearPending();

        state = SnapshotState::Uninitialized;

        live_slot    = 0;
        pending_slot = 1;

        topology_revision = 0;
        content_hash      = 0;
        frame_serial      = 0;
        collected_items   = 0;

        building = false;

        ResetStats();
    }
}//namespace hgl::ecs
