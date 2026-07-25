#include<hgl/ecs/support/PrimitiveBatchPipeline.h>
#include<source_location>
#include<cstdio>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/BoundingBoxComponent.h>
#include<hgl/ecs/systems/tick/BoundingBoxUpdateSystem.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/graph/DescriptorBindingSet.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterializationPools.h>
#include<hgl/object/ObjectTracker.h>
#include<hgl/util/hash/FNV1a.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/vk/VKIndirectCommandBuffer.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderPass.h>
#include<hgl/vk/VKVertexInputLayout.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/log/Log.h>
#include<algorithm>
#include<array>
#include<chrono>
#include<cstdint>
#include<string>
#include<unordered_map>

namespace hgl::ecs
{
    namespace
    {
        uint64_t HashDBSContractBindingSignature(const graph::DescriptorBindingSet *binding_set,
                                                 const graph::mtl::BindingContract &contract)
        {
            if (!binding_set)
                return 0;

            uint64_t hash = hgl::hash::FNV1aInit<uint64_t>();
            uint32_t count = 0;

            for (const auto &req : contract.requirements)
            {
                if (!req.required)
                    continue;

                switch (req.semantic)
                {
                case graph::mtl::DescriptorSemantic::MaterialInstance:
                case graph::mtl::DescriptorSemantic::MaterialTextureLayerTable:
                case graph::mtl::DescriptorSemantic::MaterialDataIndexTable:
                {
                    graph::DescriptorBindingSet::SSBOBinding binding{};
                    if (!binding_set->GetSSBOBinding(req.ssbo_type, binding))
                        continue;

                    hash = hgl::hash::FNV1aAppendValueBytes(hash, req.semantic);
                    hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.ssbo_type);
                    hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.ssbo_id);
                    hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.slot_index);
                    ++count;
                    break;
                }
                case graph::mtl::DescriptorSemantic::MaterialTexture:
                case graph::mtl::DescriptorSemantic::MaterialSampler:
                {
                    graph::DescriptorBindingSet::TextureBinding binding{};
                    if (!binding_set->GetTextureBinding(req.texture_slot, binding))
                        continue;

                    hash = hgl::hash::FNV1aAppendValueBytes(hash, req.semantic);
                    hash = hgl::hash::FNV1aAppendValueBytes(hash, req.texture_slot);

                    const uint64_t texture_ptr = uint64_t(uintptr_t(binding.texture));
                    const uint64_t sampler_ptr = uint64_t(uintptr_t(binding.sampler));
                    hash = hgl::hash::FNV1aAppendValueBytes(hash, texture_ptr);
                    hash = hgl::hash::FNV1aAppendValueBytes(hash, sampler_ptr);
                    ++count;
                    break;
                }
                default:
                    break;
                }
            }

            if (count == 0)
                return 0;

            hash = hgl::hash::FNV1aAppendValueBytes(hash, count);
            return hash;
        }

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
        TransformSystem* transform_system = nullptr;
        if (world)
        {
            if (auto system = world->GetSystem<TransformSystem>())
            {
                transform_system = system.get();
                transform_system->SetDevice(device);
                transform_system->EnsureTransformBuffer();
                transform_system->RefreshHandleOrder();
            }
        }

        AssignTransformIndices(transform_system);
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

    void PrimitiveBatchPipeline::AssignTransformIndices(TransformSystem* transform_system)
    {
        if (!world)
            return;

        auto& cache = world->GetRenderFrameCache();
        uint32_t static_count = 0;
        uint32_t dynamic_count = 0;
        uint32_t dynamic_base = 0;

        if (transform_system)
        {
            static_count = transform_system->GetStaticCount();
            dynamic_count = transform_system->GetDynamicCount();
            dynamic_base = transform_system->GetDynamicBaseIndex(static_count, dynamic_count);
        }

        for (auto& itemPtr : cache.renderItems)
        {
            RenderItem* item = itemPtr.get();
            if (!item)
                continue;

            auto transform = item->GetTransform();
            if (!transform)
                continue;

            const auto handle = transform->GetStorageHandle();
            if (handle == TransformDataStorage::INVALID_HANDLE)
            {
                item->transform_index = 0;
                continue;
            }

            uint32_t group_index = 0;
            if (transform_system &&
                transform_system->TryGetTransformGroupIndex(handle, transform->IsMovable(), group_index))
            {
                const PositionSourceSpec position_source_spec = item->GetPositionSourceSpec();
                const TransformPolicySpec transform_policy_spec = item->GetTransformPolicySpec();

                switch (position_source_spec)
                {
                case PositionSourceSpec::MeshVertex:
                case PositionSourceSpec::Quad2DGenerated:
                case PositionSourceSpec::TerrainHeightmapGrid:
                case PositionSourceSpec::ProceduralGenerated:
                default:
                    switch (transform_policy_spec.facing)
                    {
                    case FacingPolicy::None:
                    case FacingPolicy::CameraFacing:
                    case FacingPolicy::AxisLocked:
                    default:
                        // Fixed screen-space scaling is a separate policy switch from facing.
                        if (transform_policy_spec.fixedScreenSpaceScale)
                        {
                            // R08 ingress-only stage: keep transform index behavior unchanged.
                        }
                        item->transform_index = transform->IsMovable() ? (dynamic_base + group_index) : (group_index + 1);
                        break;
                    }
                    break;
                }
            }
            else
            {
                item->transform_index = 0;
            }
        }
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
        EnsureBatchIndexRows(batch);
        WriteBatchIndexRows(batch);
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

    void PrimitiveBatchPipeline::EnsureBatchIndexRows(MaterialBatch& batch)
    {
        if (!batch.buffer_manager || batch.items.empty())
            return;

        const uint32_t item_count = static_cast<uint32_t>(batch.items.size());
        uint32_t new_node_count = 1;
        while (new_node_count < item_count)
            new_node_count <<= 1;

        // Per-batch L2W index rows SSBO — written in draw order.
        if (!batch.l2w_index_rows_buffer || batch.l2w_index_rows_capacity < item_count)
        {
            batch.l2w_index_rows_capacity = new_node_count;

            if (batch.l2w_index_rows_buffer)
            {
                if (batch.buffer_manager)
                    batch.buffer_manager->Release(batch.l2w_index_rows_buffer);
                else
                    delete batch.l2w_index_rows_buffer;
                batch.l2w_index_rows_buffer = nullptr;
            }

            if (batch.buffer_manager)
            {
                const VkDeviceSize byte_size = static_cast<VkDeviceSize>(batch.l2w_index_rows_capacity) * sizeof(uint32_t);
                batch.l2w_index_rows_buffer = batch.buffer_manager->CreateSSBO(
                    "ECS:Batch:L2WIndexRows", byte_size, nullptr, graph::SharingMode::Exclusive);
            }
        }

        // Per-batch DataIndex rows SSBO — shader consumes this as a flat uint[] by instance index.
        if (batch.key.material && batch.key.material->hasMI())
        {
            if (!batch.mi_data_index_rows_buffer || batch.mi_data_index_rows_capacity < item_count)
            {
                batch.mi_data_index_rows_capacity = new_node_count;

                if (batch.mi_data_index_rows_buffer)
                {
                    if (batch.buffer_manager)
                        batch.buffer_manager->Release(batch.mi_data_index_rows_buffer);
                    else
                        delete batch.mi_data_index_rows_buffer;
                    batch.mi_data_index_rows_buffer = nullptr;
                }

                if (batch.buffer_manager)
                {
                    const VkDeviceSize byte_size = static_cast<VkDeviceSize>(batch.mi_data_index_rows_capacity) * sizeof(uint32_t);
                    batch.mi_data_index_rows_buffer = batch.buffer_manager->CreateSSBO(
                        "ECS:Batch:MIDataIndexRows", byte_size, nullptr, graph::SharingMode::Exclusive);
                }
            }
        }

    }

    void PrimitiveBatchPipeline::WriteBatchIndexRows(MaterialBatch& batch)
    {
        if (batch.items.empty())
            return;

        const uint32_t item_count = static_cast<uint32_t>(batch.items.size());

        // Also write per-batch L2W index rows SSBO in the same draw order.
        // This is what the shader reads via ResolveTransformID(gl_InstanceIndex).
        if (batch.l2w_index_rows_buffer)
        {
            auto *l2w_gpu = batch.l2w_index_rows_buffer->GetGPUBuffer();
            if (l2w_gpu)
            {
                uint32_t *l2w_ptr = static_cast<uint32_t *>(l2w_gpu->Map(0, static_cast<VkDeviceSize>(item_count) * sizeof(uint32_t)));
                if (l2w_ptr)
                {
                    for (size_t i = 0; i < item_count; ++i)
                    {
                        RenderItem* item = batch.items[i];
                        *l2w_ptr = item ? item->transform_index : 0;
                        ++l2w_ptr;
                    }
                    l2w_gpu->Unmap();
                }
            }
        }

        // Write per-batch DataIndex rows SSBO in draw order.
        // mtl_data_index_rows is declared as uint values[] in shader, so this must be tightly packed uint.
        if (batch.mi_data_index_rows_buffer)
        {
            auto *mi_gpu = batch.mi_data_index_rows_buffer->GetGPUBuffer();
            if (mi_gpu)
            {
                graph::mtl::SSBOType primary_ssbo_type = graph::mtl::SSBOType::PBRSurface;
                if (batch.key.material)
                {
                    for (const auto &req : batch.key.material->GetBindingContract().requirements)
                    {
                        if (req.semantic == graph::mtl::DescriptorSemantic::MaterialInstance)
                        {
                            primary_ssbo_type = req.ssbo_type;
                            break;
                        }
                    }
                }

                uint32_t *row_ptr = static_cast<uint32_t *>(
                    mi_gpu->Map(0, static_cast<VkDeviceSize>(item_count) * sizeof(uint32_t)));
                if (row_ptr)
                {
                    for (size_t i = 0; i < item_count; ++i)
                    {
                        RenderItem *item = batch.items[i];
                        uint32_t mi_index = 0;
                        if (item)
                        {
                            if (auto *binding_set = item->GetDescriptorBindingSet())
                            {
                                if (binding_set->HasSSBOBinding(primary_ssbo_type))
                                    mi_index = binding_set->GetSlotIndex(primary_ssbo_type);
                            }
                            else if (auto *mi = item->GetMaterialInstance())
                            {
                                const int mi_id = mi->GetMIID();
                                // mi_id is now always -1 (legacy path, no material-owned data store).
                                // Keep mi_index at 0 so the shader reads slot 0 rather than wrapping to UINT32_MAX.
                                mi_index = (mi_id >= 0) ? static_cast<uint32_t>(mi_id) : 0u;
                            }
                        }
                        row_ptr[i] = mi_index;
                    }
                    mi_gpu->Unmap();
                }
            }
        }

    }
    void PrimitiveBatchPipeline::BuildMaterialBatches()
    {
        if (!world)
            return;

        auto& cache = world->GetRenderFrameCache();

        TransformAssignmentBuffer* shared_transform_buffer = nullptr;
        if (auto transform_system = world->GetSystem<TransformSystem>())
        {
            shared_transform_buffer = transform_system->GetTransformBuffer();
        }

        auto buffer_manager = GetBufferManager();
        std::shared_ptr<RenderDescriptorBindingSystem> descriptor_binding_system{};
        if (world)
            descriptor_binding_system = world->GetSystem<RenderDescriptorBindingSystem>();
        struct MaterialSpecCacheValue
        {
            uint64_t program_signature = 0;
            uint64_t binding_signature = 0;
            std::array<uint32_t, static_cast<size_t>(graph::mtl::TextureSlot::RANGE_SIZE)> texture_slot_handles{};
            bool has_texture_slot_handles = false;
        };

        std::unordered_map<graph::Material *, MaterialSpecCacheValue> material_spec_hash_cache;

        auto* render_ctx = world ? world->GetRenderContext() : nullptr;
        auto* rt = render_ctx ? render_ctx->GetCurrentRenderTarget() : nullptr;
        auto* current_render_pass = rt ? rt->GetRenderPass() : nullptr;

        for (auto& itemPtr : cache.renderItems)
        {
            RenderItem* item = itemPtr.get();
            if (!item || !item->isVisible)
                continue;

            auto* material = item->GetMaterial();
            auto* pipeline = item->GetPipeline();
            auto* prim_item = dynamic_cast<PrimitiveRenderItem*>(item);
            auto prim_comp = prim_item ? prim_item->GetPrimitiveComponent() : nullptr;

            // Output interface changed (different RenderPass): invalidate previous runtime-resolved pipeline.
            if (prim_comp
             && prim_comp->GetResolvedRuntimePipeline()
             && current_render_pass
             && prim_comp->GetResolvedRuntimeRenderPass() != current_render_pass)
            {
                prim_comp->ClearResolvedRuntimePipeline();
                pipeline = item->GetPipeline();
            }

            // Late-resolve pipeline: if no pre-baked or previously resolved pipeline exists
            // and the item's PrimitiveComponent has a pending preset, try to resolve now.
            if (!pipeline && prim_comp && prim_comp->HasPendingPipelinePreset() && material && current_render_pass)
            {
                auto* dbs = item->GetDescriptorBindingSet();
                const graph::VIL* vil = dbs ? dbs->GetVIL() : material->GetDefaultVIL();

                graph::Pipeline* resolved = current_render_pass->CreatePipeline(
                    material, vil,
                    prim_comp->GetPendingPipelinePreset());

                if (resolved)
                {
                    prim_comp->SetResolvedRuntimePipeline(resolved, current_render_pass);
                    pipeline = resolved;
                }
                else
                {
                    LogWarning("[PrimitiveBatchPipeline] Late-resolve pipeline failed for material %s", material->GetName().c_str());
                }
            }

            if (!material || !pipeline)
                continue;

            const auto &binding_contract = material->GetBindingContract();
            auto *binding_set = item->GetDescriptorBindingSet();
            if (binding_set)
            {
                if (!binding_set->SatisfiesContract(binding_contract, material->GetName().c_str()))
                    continue;
            }

            uint64_t program_signature = 0;
            uint64_t binding_signature = 0;
            if (descriptor_binding_system)
            {
                auto it = material_spec_hash_cache.find(material);
                if (it != material_spec_hash_cache.end())
                {
                    program_signature = it->second.program_signature;
                    binding_signature = it->second.binding_signature;
                }
                else
                {
                    graph::mtl::MaterialRecipe recipe{};
                    graph::mtl::MaterializationSpec spec{};
                    std::array<uint32_t, static_cast<size_t>(graph::mtl::TextureSlot::RANGE_SIZE)> texture_slot_handles{};
                    bool has_texture_slot_handles = false;

                    // 批处理分桶必须使用“材质级”语义，不能带实例身份（如 MIID），
                    // 否则会把本可合并的实例拆成 1 instance / batch。
                    if (descriptor_binding_system->BuildMaterialRecipeForMaterial(material, recipe))
                    {
                        if (!recipe.structs.empty() || !recipe.textures.empty())
                        {
                            const uint32_t mi_data_bytes = material->GetMIDataBytes();
                            if (mi_data_bytes > 0)
                            {
                                for (const auto &struct_ref : recipe.structs)
                                {
                                    descriptor_binding_system->RegisterMaterialStructLayout(struct_ref.ssbo_type,
                                                                                           struct_ref.ssbo_id,
                                                                                           mi_data_bytes);
                                }
                            }

                            if (descriptor_binding_system->ResolveMaterialRecipe(recipe, spec, nullptr, nullptr))
                            {
                                program_signature = graph::mtl::HashMaterializationProgramSignature(spec);
                                binding_signature = graph::mtl::HashMaterializationBindingSignature(spec);

                                for (const auto &resolved : spec.resources)
                                {
                                    const size_t slot = static_cast<size_t>(resolved.slot);
                                    if (slot >= texture_slot_handles.size())
                                        continue;

                                    texture_slot_handles[slot] = resolved.bindless_handle;
                                    if (resolved.bindless_handle != 0)
                                        has_texture_slot_handles = true;
                                }
                            }
                        }
                    }

                    MaterialSpecCacheValue cache_value{};
                    cache_value.program_signature = program_signature;
                    cache_value.binding_signature = binding_signature;
                    cache_value.texture_slot_handles = texture_slot_handles;
                    cache_value.has_texture_slot_handles = has_texture_slot_handles;
                    material_spec_hash_cache.emplace(material, std::move(cache_value));
                }
            }

            if (binding_set)
            {
                const uint64_t dbs_binding_signature = HashDBSContractBindingSignature(binding_set, binding_contract);
                if (dbs_binding_signature != 0)
                {
                    uint64_t merged_signature = hgl::hash::FNV1aInit<uint64_t>();
                    merged_signature = hgl::hash::FNV1aAppendValueBytes(merged_signature, binding_signature);
                    merged_signature = hgl::hash::FNV1aAppendValueBytes(merged_signature, dbs_binding_signature);
                    binding_signature = merged_signature;
                }
            }

            MaterialPipelineKey key(material, pipeline, program_signature, binding_signature);
            auto* batch_ptr = cache.materialBatches.GetValuePointer(key);

            if (!batch_ptr)
            {
                auto batch = std::make_unique<MaterialBatch>(key, device, buffer_manager);
                batch->cameraInfo = camera_info;
                batch->transform_buffer = shared_transform_buffer;
                if (descriptor_binding_system)
                {
                    auto cache_it = material_spec_hash_cache.find(material);
                    if (cache_it != material_spec_hash_cache.end())
                    {
                        batch->texture_slot_handles = cache_it->second.texture_slot_handles;
                        batch->has_texture_slot_handles = cache_it->second.has_texture_slot_handles;
                    }
                }
                batch->AddItem(item);
                cache.materialBatches[key] = std::move(batch);
            }
            else
            {
                (*batch_ptr)->buffer_manager = buffer_manager;
                (*batch_ptr)->transform_buffer = shared_transform_buffer;
                if (descriptor_binding_system)
                {
                    auto cache_it = material_spec_hash_cache.find(material);
                    if (cache_it != material_spec_hash_cache.end())
                    {
                        (*batch_ptr)->texture_slot_handles = cache_it->second.texture_slot_handles;
                        (*batch_ptr)->has_texture_slot_handles = cache_it->second.has_texture_slot_handles;
                    }
                }
                (*batch_ptr)->AddItem(item);
            }
        }
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
