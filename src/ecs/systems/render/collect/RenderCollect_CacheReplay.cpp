#include <hgl/ecs/systems/render/collect/RenderCollectSystem.h>
#include "RenderCollectInternal.h"

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/GPUModelRefComponent.h>
#include <hgl/ecs/support/pipeline/RenderResource.h>
#include <hgl/ecs/support/storage/FrameProfiler.h>
#include <hgl/ecs/core/render/PrimitiveRenderItem.h>
#include <hgl/ecs/core/render/ModelTemplateRenderItem.h>
#include <hgl/ecs/support/material/MaterialRuntimeTable.h>
#include <hgl/ecs/support/material/MaterialVariantTable.h>
#include <hgl/ecs/support/pipeline/RenderStrategyParity.h>
#include <hgl/graph/ssbo/ModelTemplateRows.h>
#include <hgl/graph/render/RenderContext.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/vk/VKRenderTargetSwapchain.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    using namespace hgl::ecs::detail::render_collect;

    bool RenderCollectSystem::TryCachedMainFrameCollect(const bool ambient_provider_changed)
    {
        if (!world)
            return false;

        // ── 前提①：开关（已把 P2 快照开关合成进判据）+ 只能在主帧 pass 上接管 ──
        if (!world->Runner().GetMainFrameTopology().IsCollectCacheEnabled())
            return false;

        if (!world->Runner().IsMainFrameTopologyPass())
            return false;

        // ── 前提②：天光算法没热切（provider 变 ⇒ program 身份变 ⇒ 材质链必须重跑）──
        if (ambient_provider_changed)
            return false;

        // ── 前提③：上一帧主帧收集是稳态（无任何实例在等流式资源 / 重试失败）──
        // 否则完整路径必须继续逐帧重试，不能把"启动时未就绪"永久缓存下来。
        if (!collect_cache.IsLastMainCollectSteady())
            return false;

        const PrimitiveTopologyLedger &ledger = world->Runner().GetMainFrameTopology().Ledger();

        // ── 前提④：GPU 剔除接管中 ──
        // CPU 剔除只在接管期间直接 return 而不改 items ⇒ 快照的成员集合与顺序原样成立。
        // 若本帧退回 CPU 剔除，它会按 items 逐项裁剪（并可能重排），那时成员集合必须
        // 由收集侧重新产出。
        if (!ledger.GetLastGPUCullEngaged())
            return false;

        MainFrameTopologySnapshot &snapshot = world->Runner().GetMainFrameTopology().Snapshot();

        // ── 前提⑤：快照就绪 + 代数一致 + **完整覆盖**上一次收集结果 ──
        if (!snapshot.IsReady())
            return false;

        if (snapshot.GetTopologyRevision() != ledger.GetTopologyRevision())
            return false;

        const SnapshotItemRecord *records = snapshot.GetItems();
        const int record_count = snapshot.GetItemCount();

        if (record_count <= 0 || !records)
            return false;

        // `collected_items != item_count` ⇒ 上一帧有项因不可见/几何行缺失被合批阶段过滤掉
        // ⇒ 快照不是同一套成员，复用会静默丢绘制。
        if (snapshot.GetCollectedItemCount() != static_cast<uint32_t>(record_count))
            return false;

        MaterialRuntimeTable *runtime_table = GetRuntimeTable(world);

        if (!runtime_table)
            return false;

        RenderFrameCache &cache = world->Runner().FrameCache();

        const size_t base_items = cache.renderItems.size();
        const uint32_t base_renderable = cache.renderableCount;

        // 4-ID 是**按 pass 解析**的（阴影程序/材质变体不同）；前面已限定主帧 pass，
        // 这里仍显式取一次，避免日后放宽前提时静默写错半边。
        const uint32_t pass_index = world->Runner().PassScope().IsShadowPass() ? 1u : 0u;

        bool ok = true;

        {
            FrameProfileSpan replay_span(FrameProfileSlot::CollectCacheReplay);

            for (int i = 0; i < record_count; ++i)
            {
                const SnapshotItemRecord &record = records[i];

                // 非模板成员不能重放为旧图元项，必须回退完整收集。
                if (record.entity_index == 0xFFFFFFFFu || record.member_kind == 0)
                {
                    ok = false;
                    break;
                }

                EntityID entity_id;
                entity_id.index      = record.entity_index;
                entity_id.generation = record.entity_generation;

                // 代数不符 ⇒ nullptr（陈旧句柄在此判出）
                Entity *entity = world->GetEntity(entity_id);

                if (!entity)
                {
                    ok = false;
                    break;
                }

                // P3 不再要求实体挂载 GeometryData；draw_geometry_id 已是快照权威绘制键。

                const TransformAccessor transform = world->Spatial().GetTransformByEntity(entity_id);

                if (!transform.IsValid())
                {
                    ok = false;
                    break;
                }

                // 材质共享行必须仍然活着：行被回收 ⇒ 变体/程序/管线/绑定全部失配，
                // 必须重跑完整材质链才能重建（这正是 P3 不做材质复用的边界）。

                const MaterialRuntimeRow *material_row = runtime_table->Get(record.material_row);

                if (!material_row)
                {
                    ok = false;
                    break;
                }

                if (!material_row->cached_effective_recipe.textures.empty())
                {
                    if (!EnsurePrimitiveModelTemplate(entity)
                     || snapshot.GetTopologyRevision() != ledger.GetTopologyRevision())
                    {
                        ok = false;
                        break;
                    }
                    material_row = runtime_table->Get(record.material_row);
                    if (!material_row)
                    {
                        ok = false;
                        break;
                    }
                }

                const uint32_t transform_id = transform.GetID();
                const uint32_t geometry_id  = record.draw_geometry_id;
                const auto *model_ref = entity->GetComponent<GPUModelRefComponent>();
                const uint32_t group_idx = record.member_kind - 1u;
                const ModelTemplateMaterialTable *template_table = world->RenderData().TemplateMaterials();
                const TemplateRenderBinding *binding = model_ref && template_table
                    ? template_table->FindBinding(model_ref->GetTemplateID()) : nullptr;
                if (!model_ref || model_ref->GetTemplateID() == graph::ssbo::kInvalidModelTemplateID
                    || !binding || group_idx >= static_cast<uint32_t>(binding->groups.GetCount()))
                {
                    ok = false;
                    break;
                }

                const MaterialRuntimeRow *fact_row = model_ref->IsAutomaticallyNormalized()
                    ? material_row : runtime_table->Get(binding->groups[0].runtime_row);
                if (!fact_row)
                {
                    ok = false;
                    break;
                }
                uint32_t material_id;
                uint32_t texture_id;
                ResolveMaterialFacts(world, entity_id, fact_row, material_id, texture_id);

                auto *render_target = world->Environment().GetRenderTarget();
                auto *current_render_pass = render_target ? render_target->GetRenderPass() : nullptr;
                auto *cached_pipeline = current_render_pass
                    ? world->RenderData().GetMaterialRuntimePipelineTable().Get(entity_id, current_render_pass, group_idx)
                    : nullptr;

                // 若该实体在当前 pass 下未解析过管线，则快路径不成立，主动回退完整路径重新解析
                if (!cached_pipeline)
                {
                    ok = false;
                    break;
                }

                const auto cached_pipeline_config = current_render_pass
                    ? world->RenderData().GetMaterialRuntimePipelineTable().GetConfig(entity_id, current_render_pass, group_idx)
                    : graph::mtl::MaterialPipelineConfig{};

                auto item = std::make_unique<ModelTemplateRenderItem>(
                    entity_id,
                    transform,
                    record.material_row,
                    group_idx,
                    model_ref->GetTemplateID(),
                    current_render_pass,
                    cached_pipeline,
                    cached_pipeline_config,
                    world);

                item->UpdateWorldMatrix();


                // 事实必须**逐帧重发**（不可跳过）：TRS 每帧可能变；可见性权威在
                // `VisibilityDataStorage`；`GPUModelRefComponent` 的部件掩码是裸字段、
                // 无 dirty 通知。这里写的每一项都是权威值，与完整路径逐字同值。
                // 绘制键走 `record`（快照）⇒ 快路径**不读**映射 GPU 缓冲里的 params 行。
                PublishPrimitiveFacts(entity_id, transform_id, geometry_id,
                                      material_id, texture_id, pass_index, item.get(),
                                      &record);

                cache.renderItems.push_back(std::move(item));
                cache.renderableCount++;
            }

        }// CollectCacheReplay 子槽

        if (!ok)
        {
            // 回退：把本次已建出的项全部丢弃（记住入表前的水位，不假设缓存为空）。
            // 已发布的事实行写的都是**权威值**，完整路径会再写一遍同样的值 ⇒ 无需回滚。
            cache.renderItems.resize(base_items);
            cache.renderableCount = base_renderable;

            collect_cache.RecordFallback();

            return false;
        }

        collect_cache.RecordHit(static_cast<uint64_t>(record_count));

        FrameProfiler::Count(FrameProfileCount::CollectItems, static_cast<uint32_t>(record_count));

        cache.fast_path_engaged     = true;
        cache.passed_can_render     = static_cast<uint32_t>(record_count);
        cache.collect_candidates    = static_cast<uint32_t>(record_count);
        cache.collected_items       = static_cast<uint32_t>(record_count);
        cache.is_collect_steady     = true;
        cache.has_pending_resources = false;

        return true;
    }

    void RenderCollectSystem::ObserveCollectCacheEligibility()
    {
        if (!world)
            return;

        const PrimitiveTopologyLedger &ledger = world->Runner().GetMainFrameTopology().Ledger();

        // 本 pass 是否有资格：阴影 pass / 移动性过滤 pass 共用同一份 RenderFrameCache，
        // 它们的成员集合天然不同 ⇒ 只有「无过滤的非阴影 pass」才可能是主帧快照的消费者。
        const bool is_main_swapchain_frame =
            !world->Runner().PassScope().IsShadowPass() && world->Runner().PassScope().GetMobilityFilter() < 0;

        const uint64_t topology_now = ledger.GetTopologyRevision();

        // 干跑：首次把当前代数当作「刚发布过快照」，之后只在判定为「拓扑脏」时刷新，
        // 因为那时真实实现会重建并发布一份新快照。
        collect_cache.EnsureDryRunInitialized(topology_now);

        CollectCacheInputs in;

        in.is_main_swapchain_frame = is_main_swapchain_frame;
        in.gpu_cull_engaged        = ledger.GetLastGPUCullEngaged();
        in.has_snapshot            = true;              // 干跑：假装已有快照
        in.snapshot_ready          = true;              // 干跑：假装快照就绪
        in.snapshot_topology_rev   = collect_cache.DryRunTopologyRevision();

        const CacheRejectReason reason = ledger.Evaluate(in);

        // 本 pass 非主帧时不计入统计：它本来就不参与主帧快照，算进分母会稀释命中率。
        if (is_main_swapchain_frame)
        {
            world->Runner().GetMainFrameTopology().Ledger().RecordEligibility(reason);

            if (reason != CacheRejectReason::Eligible)
                collect_cache.AdvanceDryRun(topology_now);
        }
    }
}//namespace hgl::ecs
