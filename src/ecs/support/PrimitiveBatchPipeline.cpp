#include<hgl/ecs/support/PrimitiveBatchPipeline.h>
#include<source_location>
#include<cstdio>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/BoundingBoxComponent.h>
#include<hgl/ecs/systems/tick/BoundingBoxUpdateSystem.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/object/ObjectTracker.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/vk/VKIndirectCommandBuffer.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineBuildRequest.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/ecs/support/MaterialInstanceAssignmentBuffer.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/ecs/support/PipelineResolveMetrics.h>
#include<hgl/log/Log.h>
#include<algorithm>
#include<limits>
#include<chrono>
#include<cstdint>
#include<atomic>
#include<unordered_set>
#include<unordered_map>

namespace hgl::ecs
{
    namespace
    {
        PipelineResolveCounters g_pipeline_preresolve_counters;
        std::atomic<uint64_t> g_pipeline_preresolve_skips{0};
        PipelineBatchPhaseTracker g_pipeline_batch_phase_tracker;

        void WriteICB(VkDrawIndirectCommand* draw_cmd, DrawBatch* batch)
        {
            if (!draw_cmd || !batch || !batch->geom_draw_range)
                return;

            draw_cmd->vertexCount = batch->geom_draw_range->vertex_count;
            draw_cmd->instanceCount = batch->instance_count;
            draw_cmd->firstVertex = batch->geom_draw_range->vertex_offset;
            draw_cmd->firstInstance = batch->first_instance;
        }

        void WriteICB(VkDrawIndexedIndirectCommand* indexed_draw_cmd, DrawBatch* batch)
        {
            if (!indexed_draw_cmd || !batch || !batch->geom_draw_range)
                return;

            indexed_draw_cmd->indexCount = batch->geom_draw_range->index_count;
            indexed_draw_cmd->instanceCount = batch->instance_count;
            indexed_draw_cmd->firstIndex = batch->geom_draw_range->first_index;
            indexed_draw_cmd->vertexOffset = batch->geom_draw_range->vertex_offset;
            indexed_draw_cmd->firstInstance = batch->first_instance;
        }

        RenderQueue DetermineRenderQueue(const graph::GraphicsPipeline* pipeline)
        {
            if (!pipeline)
                return RenderQueue::Opaque;

            const auto* pd = pipeline->GetData();
            if (!pd)
                return RenderQueue::Opaque;

            if (pd->depth_stencil
             && pd->depth_stencil->depthCompareOp == VK_COMPARE_OP_ALWAYS
             && pd->depth_stencil->depthWriteEnable == VK_FALSE)
            {
                return RenderQueue::Overlay;
            }

            if (pipeline->IsAlphaBlend())
                return RenderQueue::Transparent;

            if (pipeline->IsAlphaTest())
                return RenderQueue::Masked;

            return RenderQueue::Opaque;
        }
    }

    bool PrimitiveBatchPipeline::PrepareFrame(ECSContext* ctx)
    {
        if (!ctx)
            return false;

        world = ctx;
        auto& cache = world->GetRenderFrameCache();
        if (cache.renderItems.empty())
            return false;

        camera_info = cache.cameraInfo;
        if (!camera_info)
            return false;

        if (!device)
            device = world->GetGPUDevice();

        const uint32_t frame_index = world->GetFrameIndex();
        if (prepared_frame_index != frame_index)
            prepared_frame_index = frame_index;

        return true;
    }

    void PrimitiveBatchPipeline::RunCulling()
    {
        PerformFrustumCulling();
    }

    void PrimitiveBatchPipeline::RunSorting()
    {
        SortByDistance();
    }

    void PrimitiveBatchPipeline::RunTransformIndexing()
    {
        if (world)
        {
            if (auto system = world->GetSystem<TransformSystem>())
            {
                system->SetDevice(device);
                system->EnsureTransformBuffer();
                system->RefreshHandleOrder();
            }
        }
    }

    void PrimitiveBatchPipeline::RunBatching()
    {
        BuildMaterialBatches();
        FinalizeBatches();
    }

    void PrimitiveBatchPipeline::PerformFrustumCulling()
    {
        if (!world || !camera_info)
            return;

        auto& cache = world->GetRenderFrameCache();

        frustum.SetMatrix(camera_info->vp);

        for (auto& itemPtr : cache.renderItems)
        {
            RenderItem* item = itemPtr.get();
            if (!item || !item->isVisible)
                continue;

            auto entity = item->GetEntity();
            if (!entity)
                continue;

            auto bbox = entity->GetComponent<BoundingBoxComponent>();
            if (bbox)
            {
                if (bbox->HasWorldAABB())
                    item->isVisible = TestFrustumWithWorldAABB(item, bbox.get());
                else
                    item->isVisible = TestFrustumWithLocalAABB(item, bbox.get());
            }
            else
            {
                item->isVisible = TestFrustumWithBoundingSphere(item);
            }
        }
    }

    bool PrimitiveBatchPipeline::TestFrustumWithWorldAABB(RenderItem* item, const BoundingBoxComponent* bbox)
    {
        const auto& world_aabb = bbox->GetWorldAABB();
        const glm::vec3 world_center = world_aabb.GetCenter();
        const glm::vec3 world_extents = world_aabb.GetExtent();
        const float radius = glm::length(world_extents);

        return frustum.SphereIn(world_center, radius) != math::Frustum::Scope::OUTSIDE;
    }

    bool PrimitiveBatchPipeline::TestFrustumWithLocalAABB(RenderItem* item, const BoundingBoxComponent* bbox)
    {
        const glm::vec3 local_center = bbox->GetCenter();
        const glm::vec3 local_extents = bbox->GetExtents();
        const float radius = glm::length(local_extents);

        const glm::mat4 worldMat = item->GetWorldMatrix();
        const glm::vec3 world_center = glm::vec3(worldMat * glm::vec4(local_center, 1.0f));

        return frustum.SphereIn(world_center, radius) != math::Frustum::Scope::OUTSIDE;
    }

    bool PrimitiveBatchPipeline::TestFrustumWithBoundingSphere(RenderItem* item)
    {
        auto* prim_item = dynamic_cast<PrimitiveRenderItem*>(item);
        if (!prim_item)
            return true;  // No bounding sphere data for this item type — keep visible
        auto primitiveComp = prim_item->GetPrimitiveComponent();
        if (!primitiveComp)
            return false;

        auto transform = item->GetTransform();
        if (!transform)
            return false;

        glm::vec3 worldPos = transform->GetWorldPosition();
        float boundingRadius = primitiveComp->GetBoundingRadius();

        if (boundingRadius <= 0.0f)
            return true;

        return frustum.SphereIn(worldPos, boundingRadius) != math::Frustum::Scope::OUTSIDE;
    }

    void PrimitiveBatchPipeline::SortByDistance()
    {
        if (!world)
            return;

        auto& cache = world->GetRenderFrameCache();

        std::sort(cache.renderItems.begin(), cache.renderItems.end(),
            [](const std::unique_ptr<RenderItem>& a, const std::unique_ptr<RenderItem>& b) {
                return a->distanceToCamera < b->distanceToCamera;
            });
    }

    graph::BufferManager* PrimitiveBatchPipeline::GetBufferManager() const
    {
        auto render_ctx = world ? world->GetRenderContext() : nullptr;
        auto graphics_context = render_ctx ? render_ctx->GetGraphicsContext() : nullptr;
        if (!graphics_context && world)
            graphics_context = world->GetGraphicsContext();
        return graphics_context ? graphics_context->GetBufferManager() : nullptr;
    }

    std::pair<graph::ObjectNameBuilder, graph::ObjectNameBuilder>
    PrimitiveBatchPipeline::BuildICBNames() const
    {
        graph::ObjectNameBuilder draw_name;
        graph::ObjectNameBuilder indexed_name;

        if (world && !world->GetResourceNamePrefix().empty())
        {
            std::string draw_str = world->GetResourceNamePrefix() + ":IndirectDrawBuffer";
            std::string indexed_str = world->GetResourceNamePrefix() + ":IndirectDrawIndexedBuffer";

            draw_name = graph::ObjectNameBuilder(draw_str.c_str());
            indexed_name = graph::ObjectNameBuilder(indexed_str.c_str());
        }
        else
        {
            draw_name = graph::ObjectNameBuilder("RPBS_IndirectDrawBuffer");
            indexed_name = graph::ObjectNameBuilder("RPBS_IndirectDrawIndexedBuffer");
        }

        return {draw_name, indexed_name};
    }

    void PrimitiveBatchPipeline::ReallocICB(MaterialBatch& batch)
    {
        HGL_CAPTURE_SCOPE();
        if (!device || batch.items.empty())
        {
            LogWarning("[ECS::PrimitiveBatchPipeline] Cannot allocate ICB - Device: %p, Items: %zu",
                       (void*)device,
                       batch.items.size());
            return;
        }

        uint32_t icb_new_count = 1;
        while (icb_new_count < batch.items.size())
            icb_new_count <<= 1;

        if (batch.icb_draw && icb_new_count <= batch.icb_draw->GetMaxCount())
            return;

        if (batch.icb_draw)
            delete batch.icb_draw;

        if (batch.icb_draw_indexed)
            delete batch.icb_draw_indexed;

        auto [draw_name, indexed_name] = BuildICBNames();

        batch.icb_draw = device->CreateIndirectDrawBuffer(icb_new_count, draw_name);
        batch.icb_draw_indexed = device->CreateIndirectDrawIndexedBuffer(icb_new_count, indexed_name);
    }

    void PrimitiveBatchPipeline::BuildBatches(MaterialBatch& batch, const uint32_t base_instance)
    {
        const size_t count = batch.items.size();
        if (count == 0)
        {
            batch.draw_batches_count = 0;
            batch.draw_batches.clear();
            return;
        }

        if (!device)
        {
            batch.draw_batches_count = 0;
            batch.draw_batches.clear();
            return;
        }

        ReallocICB(batch);

        if (!batch.icb_draw || !batch.icb_draw_indexed)
        {
            batch.draw_batches_count = 0;
            batch.draw_batches.clear();
            return;
        }

        VkDrawIndirectCommand* draw_cmd = batch.icb_draw->MapCmd();
        VkDrawIndexedIndirectCommand* indexed_draw_cmd = batch.icb_draw_indexed->MapCmd();

        if (!draw_cmd || !indexed_draw_cmd)
        {
            batch.icb_draw->Unmap();
            batch.icb_draw_indexed->Unmap();
            batch.draw_batches_count = 0;
            batch.draw_batches.clear();
            return;
        }

        batch.draw_batches.clear();
        batch.draw_batches.resize(count);

        DrawBatch* draw_batch = batch.draw_batches.data();
        RenderItem* item = batch.items[0];
        graph::Primitive* primitive = item ? item->GetPrimitive() : nullptr;

        if (!primitive)
        {
            batch.icb_draw->Unmap();
            batch.icb_draw_indexed->Unmap();
            batch.draw_batches_count = 0;
            batch.draw_batches.clear();
            return;
        }

        batch.draw_batches_count = 1;
        draw_batch->first_instance = base_instance;
        draw_batch->instance_count = 1;
        draw_batch->Set(primitive);

        const graph::GeometryDataBuffer* current_data_buffer = draw_batch->geom_data_buffer;
        const graph::GeometryDrawRange* current_draw_range = draw_batch->geom_draw_range;

        for (size_t i = 1; i < count; i++)
        {
            item = batch.items[i];
            primitive = item ? item->GetPrimitive() : nullptr;

            if (!primitive)
                continue;

            const graph::GeometryDataBuffer* item_data_buf = primitive->GetDataBuffer();
            const graph::GeometryDrawRange* item_draw_range = primitive->GetRenderData();

            if (*current_data_buffer == *item_data_buf &&
                *current_draw_range == *item_draw_range)
            {
                ++draw_batch->instance_count;
                continue;
            }

            if (draw_batch->geom_data_buffer && draw_batch->geom_data_buffer->vdm)
            {
                if (draw_batch->geom_data_buffer->ibo)
                    WriteICB(indexed_draw_cmd++, draw_batch);
                else
                    WriteICB(draw_cmd++, draw_batch);
            }

            ++batch.draw_batches_count;
            ++draw_batch;

            draw_batch->first_instance = base_instance + static_cast<uint32_t>(i);
            draw_batch->instance_count = 1;
            draw_batch->Set(primitive);

            current_data_buffer = draw_batch->geom_data_buffer;
            current_draw_range = draw_batch->geom_draw_range;
        }

        if (draw_batch->geom_data_buffer && draw_batch->geom_data_buffer->vdm)
        {
            if (draw_batch->geom_data_buffer->ibo)
                WriteICB(indexed_draw_cmd, draw_batch);
            else
                WriteICB(draw_cmd, draw_batch);
        }

        batch.icb_draw->Unmap();
        batch.icb_draw_indexed->Unmap();

        {
            static uint32_t s_batch_result_tick = 0;
            if (++s_batch_result_tick <= 3u)
                LogDebug("[BatchPipeline::BuildBatches] result: draw_batches=%u items=%zu first_instance_count=%u",
                         batch.draw_batches_count,
                         count,
                         batch.draw_batches_count > 0 ? batch.draw_batches[0].instance_count : 0u);
        }
    }

    void PrimitiveBatchPipeline::FinalizeBatch(MaterialBatch& batch)
    {
        SortBatchItems(batch);
        BuildBatches(batch, 0);
        if (!batch.transform_buffer && !batch.items.empty())
            batch.transform_buffer = new TransformAssignmentBuffer(batch.buffer_manager, TransformAssignmentBuffer::Mode::MovableOnly);

        if (batch.transform_buffer && !batch.items.empty())
            batch.transform_buffer->WriteItems(batch.items);

        UpdateMaterialInstanceBuffer(batch);
    }

    void PrimitiveBatchPipeline::SortBatchItems(MaterialBatch& batch)
    {
        std::vector<RenderItem*> static_items;
        std::vector<RenderItem*> movable_items;
        static_items.reserve(batch.items.size());
        movable_items.reserve(batch.items.size());

        for (auto *item : batch.items)
        {
            if (!item)
                continue;

            auto transform = item->GetTransform();
            if (transform && !transform->IsMovable())
                static_items.push_back(item);
            else
                movable_items.push_back(item);
        }

        std::sort(static_items.begin(), static_items.end(),
            [](const RenderItem* a, const RenderItem* b) {
                return a->Compare(*b) < 0;
            });

        std::sort(movable_items.begin(), movable_items.end(),
            [](const RenderItem* a, const RenderItem* b) {
                return a->Compare(*b) < 0;
            });

        batch.items.clear();
        batch.items.reserve(static_items.size() + movable_items.size());
        for (auto *item : static_items)
            batch.items.push_back(item);
        for (auto *item : movable_items)
            batch.items.push_back(item);

        for (size_t i = 0; i < batch.items.size(); ++i)
        {
            batch.items[i]->index = i;
        }

        batch.static_count = static_cast<uint32_t>(static_items.size());
    }

    void PrimitiveBatchPipeline::UpdateMaterialInstanceBuffer(MaterialBatch& batch)
    {
        if (!batch.key.material || !batch.key.material->HasInstanceData())
            return;

        if (!batch.mi_buffer && !batch.items.empty())
            batch.mi_buffer = new MaterialInstanceAssignmentBuffer(batch.buffer_manager, batch.key.material);

        bool domain_direct_enabled = false;
        if (world)
        {
            auto collect_system = world->GetSystem<RenderPrimitiveCollectSystem>();
            if (collect_system)
                domain_direct_enabled = collect_system->GetDomainDirectMISsboEnabled();
        }

        if (batch.mi_buffer)
            batch.mi_buffer->SetUseResolvedDomainSlotID(domain_direct_enabled);

        if (batch.mi_buffer && !batch.items.empty())
            batch.mi_buffer->WriteItems(batch.items);

        {
            static uint32_t s_upd_tick = 0;
            if (++s_upd_tick <= 3u)
                LogDebug("[BatchPipeline::UpdateMIBuf] material=%p(%s) domain_id=%u HasInstanceData=%d mi_buf=%p items=%zu",
                         (void*)batch.key.material,
                         batch.key.material ? batch.key.material->GetName().c_str() : "null",
                         batch.key.idd_handle.id,
                         batch.key.material ? (int)batch.key.material->HasInstanceData() : -1,
                         (void*)batch.mi_buffer,
                         batch.items.size());
        }
    }

    void PrimitiveBatchPipeline::BuildMaterialBatches()
    {
        if (!world)
            return;

        bool domain_direct_enabled = false;
        if (auto collect_system = world->GetSystem<RenderPrimitiveCollectSystem>())
            domain_direct_enabled = collect_system->GetDomainDirectMISsboEnabled();

        const uint64_t vkcreate_at_start = graph::RenderTargetFormat::GetVkCreateCount();
        const uint64_t outside = ComputeOutsideBatchPipelineCreation(vkcreate_at_start,
                                                                     g_pipeline_batch_phase_tracker);
        if (outside > 0)
        {
            LogWarning("[ECS::PrimitiveBatchPipeline] Stage-4 violation: %llu pipeline(s) created OUTSIDE batch phase (between last batch-end and this batch-start). Hot-path pipeline creation must be eliminated.",
                       static_cast<unsigned long long>(outside));
        }
        auto& cache = world->GetRenderFrameCache();

        auto buffer_manager = GetBufferManager();
        auto* render_target = world->GetRenderTarget();
        auto* render_format = render_target ? render_target->GetRenderFormat() : nullptr;

        uint32_t frame_attempts = 0;
        uint32_t frame_successes = 0;
        uint32_t frame_failures = 0;
        uint32_t frame_skips = 0;
        uint32_t frame_items_considered = 0;
        uint32_t frame_items_resolved_slot = 0;
        uint32_t frame_items_primitive_slot = 0;
        uint32_t frame_domain_direct_fallback = 0;
        uint32_t frame_fallback_no_snapshot = 0;
        uint32_t frame_fallback_snapshot_no_material = 0;
        uint32_t frame_fallback_snapshot_no_domain = 0;
        uint32_t frame_fallback_snapshot_no_mi = 0;
        // Phase 3 diagnostic: VIL sourced from primitive vs. fallback to material default.
        uint32_t frame_vil_from_prim    = 0;
        uint32_t frame_vil_from_default = 0;  // "fallback_count"
        // Phase 3 diagnostic: per-material set of VIL attrib counts (layout diversity).
        std::unordered_map<graph::MaterialTemplate*, std::unordered_set<uint32_t>> material_vil_layouts;
        std::unordered_set<graph::Primitive*> seen_primitives;

        for (auto& itemPtr : cache.renderItems)
        {
            RenderItem* item = itemPtr.get();
            if (!item || !item->isVisible)
                continue;

            const auto& binding = item->GetEntityMaterialBinding();

            ++frame_items_considered;

            auto* primitive = item->GetPrimitive();
            const bool use_resolved_slot = domain_direct_enabled && binding.IsDrawBindingValid();
            if (use_resolved_slot)
                ++frame_items_resolved_slot;
            else
                ++frame_items_primitive_slot;

            if (domain_direct_enabled && !use_resolved_slot)
            {
                ++frame_domain_direct_fallback;

                // Phase B telemetry: explain why an item did not qualify for
                // resolved-slot domain-direct path in this frame.
                const bool has_snapshot_signal = binding.HasSnapshotSignal();

                if (!has_snapshot_signal)
                {
                    ++frame_fallback_no_snapshot;
                }
                else if (binding.material_template == nullptr)
                {
                    ++frame_fallback_snapshot_no_material;
                }
                else if (!binding.idd_handle.IsValid())
                {
                    ++frame_fallback_snapshot_no_domain;
                }
                else if (binding.slot_id < 0)
                {
                    ++frame_fallback_snapshot_no_mi;
                }
            }

            auto* material = use_resolved_slot
                           ? binding.material_template
                           : item->GetMaterial();

            const graph::IDDHandle idd_handle = use_resolved_slot
                         ? binding.idd_handle
                         : (primitive ? primitive->GetDomainHandle() : graph::IDDHandle{});

            const graph::VIL* effective_vil = nullptr;
            graph::GraphicsPipelinePreset preset = graph::GraphicsPipelinePreset::Solid3D;

            if (primitive)
                preset = primitive->GetRenderPreset();

            if (use_resolved_slot)
                effective_vil = binding.vil;
            else if (primitive)
                effective_vil = primitive->GetVIL();

            graph::GraphicsPipeline* pipeline = nullptr;

            if (primitive && !use_resolved_slot)
            {
                seen_primitives.insert(primitive);

                auto it = resolved_pipeline_cache.find(primitive);
                if (it != resolved_pipeline_cache.end())
                    pipeline = it->second;
            }

            // Stage-4 prework: resolve pipeline before renderer hot path.
            // Resolve from Primitive-owned material state instead of depending on an existing pipeline.
            if (material && render_format)
            {
                const graph::GraphicsPipelineData* pipeline_data = nullptr;
                bool prim_restart = false;

                if (!effective_vil)
                {
                    effective_vil = material->GetDefaultVIL();
                    ++frame_vil_from_default;  // fallback_count increment
                }
                else
                {
                    ++frame_vil_from_prim;
                }

                // Phase 3: record layout diversity (vil attrib count as compact key).
                if (effective_vil && material)
                    material_vil_layouts[material].insert(effective_vil->GetVertexAttribCount());

                pipeline_data = graph::GetGraphicsPipelineData(preset);
                if (pipeline_data)
                    prim_restart = (pipeline_data->input_assembly.primitiveRestartEnable == VK_TRUE);

                if (effective_vil && pipeline_data && device)
                {
                    ++frame_attempts;
                    RecordPipelineResolveAttempt(g_pipeline_preresolve_counters);

                    graph::GraphicsPipelineBuildRequest req;
                    req.material = material;
                    req.vil = effective_vil;
                    req.render_format = render_format;
                    req.pipeline_data = pipeline_data;
                    req.primitive = material->GetPrimitiveType();
                    req.primitive_restart = prim_restart;

                    if (graph::GraphicsPipeline* acquired = device->AcquireGraphicsPipeline(req))
                    {
                        pipeline = acquired;
                        if (primitive && !use_resolved_slot)
                            resolved_pipeline_cache[primitive] = acquired;
                        ++frame_successes;
                        RecordPipelineResolveSuccess(g_pipeline_preresolve_counters);
                    }
                    else
                    {
                        ++frame_failures;
                        const uint64_t failures_total = RecordPipelineResolveFailure(g_pipeline_preresolve_counters);

                        if (ShouldLogPow2(failures_total))
                        {
                            // Phase 3 diagnostic fields: include vil_hash (attrib count) at failure site.
                            const uint32_t vil_attrib_count = effective_vil ? effective_vil->GetVertexAttribCount() : 0u;
                            LogWarning("[ECS::PrimitiveBatchPipeline] GraphicsPipeline pre-resolve failed: "
                                       "frame_failures=%u total_failures=%llu material=%s preset=%d "
                                       "vil_attrib_count=%u vil=%p",
                                       frame_failures,
                                       static_cast<unsigned long long>(failures_total),
                                       material->GetName().c_str(),
                                       int(preset),
                                       vil_attrib_count,
                                       static_cast<const void*>(effective_vil));
                        }
                        // Keep original pipeline to preserve rendering continuity.
                    }
                }
                else
                {
                    ++frame_skips;
                    const uint64_t skips_total = ++g_pipeline_preresolve_skips;
                    if (ShouldLogPow2(skips_total))
                    {
                        LogDebug("[ECS::PrimitiveBatchPipeline] GraphicsPipeline pre-resolve skipped: missing data (vil=%p pipeline_data=%p device=%p preset=%d), total_skips=%llu",
                                 static_cast<const void *>(effective_vil),
                                 static_cast<const void *>(pipeline_data),
                                 static_cast<const void *>(device),
                                 int(preset),
                                 static_cast<unsigned long long>(skips_total));
                    }
                }
            }

            if (!material || !pipeline)
            {
                static uint64_t s_no_draw_skip = 0;
                if (ShouldLogPow2(++s_no_draw_skip))
                {
                    const bool  has_deferred = primitive && primitive->HasDeferredMI();
                    const AnsiString prim_name = primitive ? primitive->GetGeometryName() : AnsiString("(no primitive)");
                    LogWarning("[BatchPipeline] item skipped (no draw call): material=%p pipeline=%p  primitive='%s'  HasDeferredMI=%d  "
                               "total_skipped=%llu  — if HasDeferredMI=1 and material=null, enable SetSemanticRuntimeResolveEnabled(true).",
                               static_cast<const void *>(material),
                               static_cast<const void *>(pipeline),
                               prim_name.c_str(),
                               (int)has_deferred,
                               static_cast<unsigned long long>(s_no_draw_skip));
                }
                continue;
            }

            const uint32_t mit_word_count = use_resolved_slot
                                          ? binding.mit_count
                                          : (primitive ? (primitive->GetMITDataBytes() / sizeof(uint32_t)) : 0u);

            if (material->GetTextureArraySlotFlags() != 0 && mit_word_count == 0)
            {
                static uint64_t s_missing_mit_payload = 0;
                if (ShouldLogPow2(++s_missing_mit_payload))
                {
                    const AnsiString prim_name = primitive->GetGeometryName();
                    LogError("[BatchPipeline] array-sampler material requires MIT payload, but primitive has none: material=%p(%s) prim='%s' array_slot_flags=0x%02X slot_id=%d total_errors=%llu",
                             static_cast<const void *>(material),
                             material->GetName().c_str(),
                             prim_name.c_str(),
                             unsigned(material->GetTextureArraySlotFlags()),
                             use_resolved_slot ? binding.slot_id : primitive->GetSlotID(),
                             static_cast<unsigned long long>(s_missing_mit_payload));
                }
            }

            // Phase 4: include InstanceDataDomain in batch key so items from different
            // domains are never merged into the same batch (nullptr = default domain).
            const RenderQueue queue = DetermineRenderQueue(pipeline);

            MaterialPipelineKey key(material, pipeline, idd_handle, queue);
            auto it = cache.materialBatches.find(key);

            if (it == cache.materialBatches.end())
            {
                auto batch = std::make_unique<MaterialBatch>(key, device, buffer_manager);
                batch->cameraInfo = camera_info;
                batch->AddItem(item);
                cache.materialBatches.emplace(key, std::move(batch));
            }
            else
            {
                it->second->buffer_manager = buffer_manager;
                it->second->AddItem(item);
            }
        }

        for (auto it = resolved_pipeline_cache.begin(); it != resolved_pipeline_cache.end();)
        {
            if (seen_primitives.find(it->first) == seen_primitives.end())
                it = resolved_pipeline_cache.erase(it);
            else
                ++it;
        }

        static uint32_t s_resolved_path_summary_tick = 0;
        if (s_resolved_path_summary_tick < 32)
        {
            std::fprintf(stderr,
                         "[BatchPipeline::ResolvedSlotSummary] domain_direct=%u items=%u resolved_slot=%u primitive_slot=%u fallback=%u fallback_no_snapshot=%u fallback_no_material=%u fallback_no_domain=%u fallback_no_mi=%u batches=%u\n",
                         domain_direct_enabled ? 1u : 0u,
                         frame_items_considered,
                         frame_items_resolved_slot,
                         frame_items_primitive_slot,
                         frame_domain_direct_fallback,
                         frame_fallback_no_snapshot,
                         frame_fallback_snapshot_no_material,
                         frame_fallback_snapshot_no_domain,
                         frame_fallback_snapshot_no_mi,
                         static_cast<uint32_t>(cache.materialBatches.size()));
        }
        ++s_resolved_path_summary_tick;

        // Diagnostics: detect if one material is split across multiple domains in one frame.
        {
            std::unordered_map<graph::MaterialTemplate *, std::unordered_set<uint32_t>> material_domains;
            uint32_t total_items_in_batches = 0;

            for (const auto &pair : cache.materialBatches)
            {
                auto *mat = pair.first.material;
                const uint32_t dom_id = pair.first.idd_handle.id;
                if (mat)
                    material_domains[mat].insert(dom_id);

                if (pair.second)
                    total_items_in_batches += static_cast<uint32_t>(pair.second->items.size());
            }

            uint32_t split_material_count = 0;
            for (const auto &it : material_domains)
            {
                if (it.second.size() > 1)
                    ++split_material_count;
            }

            if (split_material_count > 0)
            {
                LogWarning("[ECS::PrimitiveBatchPipeline] material-domain split detected: materials_with_multi_domain=%u total_batches=%u total_items=%u",
                           split_material_count,
                           static_cast<uint32_t>(cache.materialBatches.size()),
                           total_items_in_batches);

                std::unordered_map<uint64_t, std::unordered_set<graph::InstanceDataDomain *>> semantic_domains;
                std::unordered_map<uint64_t, uint32_t> semantic_item_counts;

                for (const auto &itemPtr : cache.renderItems)
                {
                    if (!itemPtr || !itemPtr->isVisible)
                        continue;

                    auto *prim_item = dynamic_cast<PrimitiveRenderItem *>(itemPtr.get());
                    if (!prim_item)
                        continue;

                    auto pc = prim_item->GetPrimitiveComponent();
                    if (!pc)
                        continue;

                    const uint64_t semantic_id = pc->GetSemanticMaterial();
                    if (semantic_id == 0)
                        continue;

                    auto *prim_diag = pc ? pc->GetPrimitive() : nullptr;
                    auto *dom = prim_diag ? prim_diag->GetDomain() : nullptr;

                    semantic_domains[semantic_id].insert(dom);
                    ++semantic_item_counts[semantic_id];
                }

                uint32_t semantic_split_count = 0;
                for (const auto &it : semantic_domains)
                {
                    if (it.second.size() > 1)
                        ++semantic_split_count;
                }

                if (semantic_split_count > 0)
                {
                    LogWarning("[ECS::PrimitiveBatchPipeline] semantic-domain split detected: semantics_with_multi_domain=%u",
                               semantic_split_count);

                    uint32_t semantic_logged = 0;
                    for (const auto &it : semantic_domains)
                    {
                        if (it.second.size() <= 1)
                            continue;

                        auto cnt_it = semantic_item_counts.find(it.first);
                        const uint32_t item_count = cnt_it != semantic_item_counts.end() ? cnt_it->second : 0;

                        LogWarning("[ECS::PrimitiveBatchPipeline] semantic-split: semantic_id=%llu domain_count=%zu item_count=%u",
                                   static_cast<unsigned long long>(it.first),
                                   it.second.size(),
                                   item_count);

                        if (++semantic_logged >= 8)
                            break;
                    }
                }

                uint32_t logged = 0;
                for (const auto &itemPtr : cache.renderItems)
                {
                    if (!itemPtr || !itemPtr->isVisible)
                        continue;

                    auto *mat = itemPtr->GetMaterial();
                    auto *prim_diag = itemPtr->GetPrimitive();
                    auto *dom = prim_diag ? prim_diag->GetDomain() : nullptr;

                    if (!mat)
                        continue;

                    auto it = material_domains.find(mat);
                    if (it == material_domains.end() || it->second.size() <= 1)
                        continue;

                    uint64_t runtime_entity_key = ToRuntimeEntityKey(itemPtr->GetEntityID());
                    uint64_t semantic_id = 0;
                    if (auto *prim_item = dynamic_cast<PrimitiveRenderItem *>(itemPtr.get()))
                    {
                        auto pc = prim_item->GetPrimitiveComponent();
                        if (pc)
                            semantic_id = pc->GetSemanticMaterial();
                    }

                    LogWarning("[ECS::PrimitiveBatchPipeline] split-sample: entity_key=%llu semantic_id=%llu material=%p prim=%p domain=%p",
                               static_cast<unsigned long long>(runtime_entity_key),
                               static_cast<unsigned long long>(semantic_id),
                               static_cast<void *>(mat),
                               static_cast<void *>(prim_diag),
                               static_cast<void *>(dom));

                    if (++logged >= 8)
                        break;
                }
            }
            else if (cache.materialBatches.size() > 0)
            {
                static uint32_t s_batch_diag_tick = 0;
                ++s_batch_diag_tick;
                if ((s_batch_diag_tick & 63u) == 1u)
                {
                    LogDebug("[ECS::PrimitiveBatchPipeline] batch summary: batches=%u items=%u visible_items=%zu",
                             static_cast<uint32_t>(cache.materialBatches.size()),
                             total_items_in_batches,
                             cache.renderItems.size());
                }
            }
        }

        // Phase 3: layout-diversity summary — log materials that legitimately use multiple VIL layouts.
        {
            static uint64_t s_diversity_frame = 0;
            const uint64_t df = ++s_diversity_frame;
            const bool should_log_diversity = (df <= 5) || ((df & (df - 1)) == 0);

            uint32_t multi_layout_materials = 0;
            for (const auto &entry : material_vil_layouts)
            {
                if (entry.second.size() > 1)
                    ++multi_layout_materials;
            }

            if (should_log_diversity || multi_layout_materials > 0)
            {
                LogDebug("[ECS::PrimitiveBatchPipeline] Phase3 layout-diversity #%llu: "
                         "vil_from_prim=%u vil_fallback=%u "
                         "unique_material_layout_combos=%zu multi_layout_materials=%u",
                         static_cast<unsigned long long>(df),
                         frame_vil_from_prim,
                         frame_vil_from_default,
                         material_vil_layouts.size(),
                         multi_layout_materials);
            }
        }

        const uint64_t vkcreate_at_end = graph::RenderTargetFormat::GetVkCreateCount();
        const uint64_t vkcreate_this_batch = vkcreate_at_end - vkcreate_at_start;
        const uint64_t skips_total = g_pipeline_preresolve_skips.load();
        uint64_t successes_total = 0;
        uint64_t failures_total = 0;
        if (ShouldLogPipelineResolveFrameSummaryWithSkips(frame_attempts,
                                                          frame_failures,
                                                          frame_skips,
                                                          g_pipeline_preresolve_counters,
                                                          skips_total,
                                                          successes_total,
                                                          failures_total))
        {
            LogDebug("[ECS::PrimitiveBatchPipeline] Pre-resolve summary: attempts=%u success=%u fail=%u skip=%u totals(s=%llu,f=%llu,k=%llu) vkcreate_this_batch=%llu",
                     frame_attempts,
                     frame_successes,
                     frame_failures,
                     frame_skips,
                     static_cast<unsigned long long>(successes_total),
                     static_cast<unsigned long long>(failures_total),
                     static_cast<unsigned long long>(skips_total),
                     static_cast<unsigned long long>(vkcreate_this_batch));
        }

        EndPipelineBatchPhase(g_pipeline_batch_phase_tracker, vkcreate_at_end);
    }

    void PrimitiveBatchPipeline::FinalizeBatches()
    {
        if (!world)
            return;

        auto& cache = world->GetRenderFrameCache();

        for (auto& pair : cache.materialBatches)
        {
            if (pair.second)
                FinalizeBatch(*pair.second);
        }
    }
}
