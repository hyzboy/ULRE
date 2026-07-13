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
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/object/ObjectTracker.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/vk/VKRenderAssign.h>
#include<hgl/vk/VKIndirectCommandBuffer.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/ecs/support/MaterialInstanceAssignmentBuffer.h>
#include<hgl/log/Log.h>
#include<algorithm>
#include<limits>
#include<chrono>
#include<cstdint>
#include<string>
#include<unordered_map>

namespace hgl::ecs
{
    namespace
    {
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

        bool HasMaterialInstanceSemantic(const graph::mtl::BindingContract &contract)
        {
            for (const auto &req : contract.requirements)
            {
                if (req.semantic == graph::mtl::DescriptorSemantic::MaterialInstance)
                    return true;
            }

            return false;
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
                item->transform_index = transform->IsMovable() ? (dynamic_base + group_index) : (group_index + 1);
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
        UpdateMaterialInstanceBuffer(batch);
        EnsureTransformVAB(batch);
        WriteTransformIndices(batch);
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

        bool has_material_instance_semantic = false;
        const auto &contract = batch.key.material->GetBindingContract();
        has_material_instance_semantic = HasMaterialInstanceSemantic(contract);

        if (!has_material_instance_semantic)
            return;

        if (!batch.mi_buffer && !batch.items.empty())
            batch.mi_buffer = new MaterialInstanceAssignmentBuffer(batch.buffer_manager, batch.key.material);

        if (batch.mi_buffer && !batch.items.empty())
        {
            // DataIndexID = 批内 MaterialInstance 索引；TextureLayerID 默认 0（由 shader 侧兼容逻辑兜底）。
            // Phase 4: Probe/Resolve 已在 BuildMaterialBatches() 前置执行，此处仅写实例索引缓冲。
            batch.mi_buffer->WriteItems(batch.items);
        }
    }

    void PrimitiveBatchPipeline::EnsureTransformVAB(MaterialBatch& batch)
    {
        if (!batch.buffer_manager || batch.items.empty())
            return;

        const uint32_t item_count = static_cast<uint32_t>(batch.items.size());
        uint32_t new_node_count = 1;
        while (new_node_count < item_count)
            new_node_count <<= 1;

        if (!batch.transform_vab || batch.transform_vab_node_count < item_count)
        {
            batch.transform_vab_node_count = new_node_count;

            if (batch.transform_vab)
            {
                if (batch.buffer_manager)
                    batch.buffer_manager->Release(batch.transform_vab);
                else
                    delete batch.transform_vab;
            }

            batch.transform_vab = batch.buffer_manager->CreateVAB(graph::Assign::TransformID::VAB_FMT,
                                                                    batch.transform_vab_node_count,
                                                                    nullptr,
                                                                    graph::BufferAllocPolicy::Auto);
            batch.transform_vab_buffer = batch.transform_vab ? batch.transform_vab->GetVkBuffer() : VK_NULL_HANDLE;
        }
    }

    void PrimitiveBatchPipeline::WriteTransformIndices(MaterialBatch& batch)
    {
        if (!batch.transform_vab || batch.items.empty())
            return;

        const uint32_t item_count = static_cast<uint32_t>(batch.items.size());
        auto *transform_gpu = batch.transform_vab->GetGPUBuffer();
        if (!transform_gpu)
        {
            LogError("[ECS::PrimitiveBatchPipeline] TransformID VAB GPU buffer is null (items=%u)", item_count);
            return;
        }

        graph::Assign::TransformID::ValueType* transform_ptr =
            (graph::Assign::TransformID::ValueType*)(batch.transform_vab->Map(0, item_count));
        if (!transform_ptr)
        {
            LogWarning("[ECS::PrimitiveBatchPipeline] TransformID VAB map failed (items=%u bytes=%llu)",
                       item_count,
                       static_cast<unsigned long long>(transform_gpu->GetSize()));
            return;
        }

        const uint32_t max_transform_id =
            std::numeric_limits<graph::Assign::TransformID::ValueType>::max();
        bool warned_overflow = false;

        for (size_t i = 0; i < batch.items.size(); ++i)
        {
            RenderItem* item = batch.items[i];
            const uint32_t idx = item ? item->transform_index : 0;

            if (idx > max_transform_id)
            {
                if (!warned_overflow && sizeof(graph::Assign::TransformID::ValueType) == sizeof(uint16_t))
                {
                    LogWarning("[ECS::PrimitiveBatchPipeline] TransformID overflow (%u)", idx);
                    warned_overflow = true;
                }
                *transform_ptr = static_cast<graph::Assign::TransformID::ValueType>(0);
            }
            else
            {
                *transform_ptr = static_cast<graph::Assign::TransformID::ValueType>(idx);
            }

            ++transform_ptr;
        }

        batch.transform_vab->Unmap();

        LogInfo("[ECS::PrimitiveBatchPipeline] TransformID VAB write complete: items=%u dirty=%d vkbuf=0x%llX",
                 item_count,
                 transform_gpu->IsDirty() ? 1 : 0,
                 static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(batch.transform_vab_buffer)));
        std::fprintf(stderr,
                 "[ECS::PrimitiveBatchPipeline] TransformID VAB write complete: items=%u dirty=%d vkbuf=0x%llX\n",
                 item_count,
                 transform_gpu->IsDirty() ? 1 : 0,
                 static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(batch.transform_vab_buffer)));
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
        std::unordered_map<graph::Material *, uint64_t> material_spec_hash_cache;

        for (auto& itemPtr : cache.renderItems)
        {
            RenderItem* item = itemPtr.get();
            if (!item || !item->isVisible)
                continue;

            auto* material = item->GetMaterial();
            auto* pipeline = item->GetPipeline();

            if (!material || !pipeline)
                continue;

            uint64_t spec_hash = 0;
            if (descriptor_binding_system)
            {
                auto it = material_spec_hash_cache.find(material);
                if (it != material_spec_hash_cache.end())
                {
                    spec_hash = it->second;
                }
                else
                {
                    graph::mtl::MaterialRecipe recipe{};
                    graph::mtl::MaterializationSpec spec{};

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
                                                                                           struct_ref.resource_domain_id,
                                                                                           mi_data_bytes,
                                                                                           struct_ref.struct_name);
                                }
                            }

                            if (descriptor_binding_system->ResolveMaterialRecipe(recipe, spec, nullptr, nullptr))
                                spec_hash = spec.spec_hash;
                        }
                    }

                    material_spec_hash_cache.emplace(material, spec_hash);
                }
            }

            MaterialPipelineKey key(material, pipeline, spec_hash);
            auto* batch_ptr = cache.materialBatches.GetValuePointer(key);

            if (!batch_ptr)
            {
                auto batch = std::make_unique<MaterialBatch>(key, device, buffer_manager);
                batch->cameraInfo = camera_info;
                batch->transform_buffer = shared_transform_buffer;
                batch->AddItem(item);
                cache.materialBatches[key] = std::move(batch);
            }
            else
            {
                (*batch_ptr)->buffer_manager = buffer_manager;
                (*batch_ptr)->transform_buffer = shared_transform_buffer;
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
