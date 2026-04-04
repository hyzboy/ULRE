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
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/object/ObjectTracker.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/vk/VKIndirectCommandBuffer.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKMaterialInstance.h>   // Phase 4: GetDomain()
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
        if (!batch.key.material || !batch.key.material->hasMI())
            return;

        if (!batch.mi_buffer && !batch.items.empty())
            batch.mi_buffer = new MaterialInstanceAssignmentBuffer(batch.buffer_manager, batch.key.material);

        if (batch.mi_buffer && !batch.items.empty())
            batch.mi_buffer->WriteItems(batch.items);
    }

    void PrimitiveBatchPipeline::BuildMaterialBatches()
    {
        if (!world)
            return;

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
        std::unordered_set<graph::Primitive*> seen_primitives;

        for (auto& itemPtr : cache.renderItems)
        {
            RenderItem* item = itemPtr.get();
            if (!item || !item->isVisible)
                continue;

            auto* material = item->GetMaterial();
            auto* primitive = item->GetPrimitive();
            graph::GraphicsPipeline* pipeline = nullptr;

            if (primitive)
            {
                seen_primitives.insert(primitive);

                auto it = resolved_pipeline_cache.find(primitive);
                if (it != resolved_pipeline_cache.end())
                    pipeline = it->second;
            }

            // Stage-4 prework: resolve pipeline before renderer hot path.
            // Resolve from MaterialInstance preset instead of depending on an existing pipeline.
            if (material && render_format)
            {
                const graph::GraphicsPipelineData* pipeline_data = nullptr;
                const graph::VIL* vil = nullptr;
                bool prim_restart = false;
                graph::GraphicsPipelinePreset preset = graph::GraphicsPipelinePreset::Solid3D;

                auto* mi = item->GetMaterialInstance();
                if (mi)
                {
                    vil = mi->GetVIL();
                    preset = mi->GetRenderPreset();
                }

                if (!vil)
                    vil = material->GetDefaultVIL();

                pipeline_data = graph::GetGraphicsPipelineData(preset);
                if (pipeline_data)
                    prim_restart = (pipeline_data->input_assembly.primitiveRestartEnable == VK_TRUE);

                if (vil && pipeline_data && device)
                {
                    ++frame_attempts;
                    RecordPipelineResolveAttempt(g_pipeline_preresolve_counters);

                    graph::GraphicsPipelineBuildRequest req;
                    req.material = material;
                    req.vil = vil;
                    req.render_format = render_format;
                    req.pipeline_data = pipeline_data;
                    req.primitive = material->GetPrimitiveType();
                    req.primitive_restart = prim_restart;

                    if (graph::GraphicsPipeline* acquired = device->AcquireGraphicsPipeline(req))
                    {
                        pipeline = acquired;
                        if (primitive)
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
                            LogWarning("[ECS::PrimitiveBatchPipeline] GraphicsPipeline pre-resolve failed: frame_failures=%u total_failures=%llu material=%s preset=%d",
                                       frame_failures,
                                       static_cast<unsigned long long>(failures_total),
                                       material->GetName().c_str(),
                                       int(preset));
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
                                 static_cast<const void *>(vil),
                                 static_cast<const void *>(pipeline_data),
                                 static_cast<const void *>(device),
                                 int(preset),
                                 static_cast<unsigned long long>(skips_total));
                    }
                }
            }

            if (!material || !pipeline)
                continue;

            // Phase 4: include ResourceDomain in batch key so items from different
            // domains are never merged into the same batch (nullptr = default domain).
            auto* mi     = item->GetMaterialInstance();
            auto* domain = mi ? mi->GetDomain() : nullptr;
            const RenderQueue queue = DetermineRenderQueue(pipeline);

            MaterialPipelineKey key(material, pipeline, domain, queue);
            auto* batch_ptr = cache.materialBatches.GetValuePointer(key);

            if (!batch_ptr)
            {
                auto batch = std::make_unique<MaterialBatch>(key, device, buffer_manager);
                batch->cameraInfo = camera_info;
                batch->AddItem(item);
                cache.materialBatches[key] = std::move(batch);
            }
            else
            {
                (*batch_ptr)->buffer_manager = buffer_manager;
                (*batch_ptr)->AddItem(item);
            }
        }

        for (auto it = resolved_pipeline_cache.begin(); it != resolved_pipeline_cache.end();)
        {
            if (seen_primitives.find(it->first) == seen_primitives.end())
                it = resolved_pipeline_cache.erase(it);
            else
                ++it;
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
