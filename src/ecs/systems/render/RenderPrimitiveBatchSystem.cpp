#include<hgl/ecs/systems/render/RenderPrimitiveBatchSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/BoundingBoxComponent.h>
#include<hgl/ecs/systems/tick/BoundingBoxUpdateSystem.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/graph/VKIndirectCommandBuffer.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/ecs/support/ECSMaterialInstanceAssignmentBuffer.h>
#include<algorithm>
#include<iostream>
#include<limits>
#include<chrono>

namespace hgl::ecs
{
    namespace
    {
        void ReallocICB(graph::VulkanDevice* device,
                        const std::vector<RenderItem*>& list,
                        graph::IndirectDrawBuffer*& icb_draw_out,
                        graph::IndirectDrawIndexedBuffer*& icb_draw_indexed_out)
        {
            if (!device || list.empty())
            {
                std::cout << "[ECS::RenderPrimitiveBatchSystem] Cannot allocate ICB - Device: "
                          << (void*)device << ", Items: " << list.size() << std::endl;
                return;
            }

            uint32_t icb_new_count = 1;
            while (icb_new_count < list.size())
                icb_new_count <<= 1;

            if (icb_draw_out && icb_new_count <= icb_draw_out->GetMaxCount())
                return;

            if (icb_draw_out)
                delete icb_draw_out;

            if (icb_draw_indexed_out)
                delete icb_draw_indexed_out;

            icb_draw_out = device->CreateIndirectDrawBuffer(icb_new_count);
            icb_draw_indexed_out = device->CreateIndirectDrawIndexedBuffer(icb_new_count);
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

        void BuildBatches(graph::VulkanDevice* device,
                          const std::vector<RenderItem*>& list,
                          DrawBatchArray& batches,
                          uint32_t& batch_count,
                          graph::IndirectDrawBuffer*& icb_draw_out,
                          graph::IndirectDrawIndexedBuffer*& icb_draw_indexed_out,
                          const uint32_t base_instance)
        {
            const size_t count = list.size();
            if (count == 0)
            {
                batch_count = 0;
                batches.clear();
                return;
            }

            if (!device)
            {
                batch_count = 0;
                batches.clear();
                return;
            }

            ReallocICB(device, list, icb_draw_out, icb_draw_indexed_out);

            if (!icb_draw_out || !icb_draw_indexed_out)
            {
                batch_count = 0;
                batches.clear();
                return;
            }

            VkDrawIndirectCommand* draw_cmd = icb_draw_out->MapCmd();
            VkDrawIndexedIndirectCommand* indexed_draw_cmd = icb_draw_indexed_out->MapCmd();

            if (!draw_cmd || !indexed_draw_cmd)
            {
                icb_draw_out->Unmap();
                icb_draw_indexed_out->Unmap();
                batch_count = 0;
                batches.clear();
                return;
            }

            batches.clear();
            batches.resize(count);

            DrawBatch* batch = batches.data();
            RenderItem* item = list[0];
            graph::Primitive* primitive = item ? item->GetPrimitive() : nullptr;

            if (!primitive)
            {
                icb_draw_out->Unmap();
                icb_draw_indexed_out->Unmap();
                batch_count = 0;
                batches.clear();
                return;
            }

            batch_count = 1;
            batch->first_instance = base_instance;
            batch->instance_count = 1;
            batch->Set(primitive);

            const graph::GeometryDataBuffer* current_data_buffer = batch->geom_data_buffer;
            const graph::GeometryDrawRange* current_draw_range = batch->geom_draw_range;

            for (size_t i = 1; i < count; i++)
            {
                item = list[i];
                primitive = item ? item->GetPrimitive() : nullptr;

                if (!primitive)
                    continue;

                const graph::GeometryDataBuffer* item_data_buf = primitive->GetDataBuffer();
                const graph::GeometryDrawRange* item_draw_range = primitive->GetRenderData();

                if (*current_data_buffer == *item_data_buf &&
                    *current_draw_range == *item_draw_range)
                {
                    ++batch->instance_count;
                    continue;
                }

                if (batch->geom_data_buffer && batch->geom_data_buffer->vdm)
                {
                    if (batch->geom_data_buffer->ibo)
                        WriteICB(indexed_draw_cmd++, batch);
                    else
                        WriteICB(draw_cmd++, batch);
                }

                ++batch_count;
                ++batch;

                batch->first_instance = base_instance + static_cast<uint32_t>(i);
                batch->instance_count = 1;
                batch->Set(primitive);

                current_data_buffer = batch->geom_data_buffer;
                current_draw_range = batch->geom_draw_range;
            }

            if (batch->geom_data_buffer && batch->geom_data_buffer->vdm)
            {
                if (batch->geom_data_buffer->ibo)
                    WriteICB(indexed_draw_cmd, batch);
                else
                    WriteICB(draw_cmd, batch);
            }

            icb_draw_out->Unmap();
            icb_draw_indexed_out->Unmap();
        }

        void FinalizeBatch(MaterialBatch& batch)
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

            BuildBatches(batch.device,
                         batch.items,
                         batch.draw_batches,
                         batch.draw_batches_count,
                         batch.icb_draw,
                         batch.icb_draw_indexed,
                         0);

            if (batch.key.material && batch.key.material->hasMI())
            {
                if (!batch.mi_buffer && !batch.items.empty())
                    batch.mi_buffer = new ECSMaterialInstanceAssignmentBuffer(batch.device, batch.key.material);

                if (batch.mi_buffer && !batch.items.empty())
                    batch.mi_buffer->WriteItems(batch.items);
            }

            if (batch.device && !batch.items.empty())
            {
                const uint32_t item_count = static_cast<uint32_t>(batch.items.size());
                uint32_t new_node_count = 1;
                while (new_node_count < item_count)
                    new_node_count <<= 1;

                if (!batch.transform_vab || batch.transform_vab_node_count < item_count)
                {
                    batch.transform_vab_node_count = new_node_count;

                    if (batch.transform_vab)
                        delete batch.transform_vab;

                    batch.transform_vab = batch.device->CreateVAB(graph::Assign::TransformID::VAB_FMT,
                                                                  batch.transform_vab_node_count,
                                                                  nullptr,
                                                                  graph::BufferAllocPolicy::Auto);
                    batch.transform_vab_buffer = batch.transform_vab ? batch.transform_vab->GetBuffer() : VK_NULL_HANDLE;
                }

                if (batch.transform_vab)
                {
                    graph::Assign::TransformID::ValueType* transform_ptr =
                        (graph::Assign::TransformID::ValueType*)(batch.transform_vab->Map(0, item_count));
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
                                std::cout << "[ECS::RenderPrimitiveBatchSystem] WARNING: TransformID overflow ("
                                          << idx << ")" << std::endl;
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
                }
            }
        }
    }

    RenderPrimitiveBatchSystem::RenderPrimitiveBatchSystem(const std::string& name)
        : System(name)
    {
        // Set system type and properties
        SetSystemType(SystemType::RenderBatch);
        SetExecutionOrder(ExecutionPhase::RenderBatch);
        
        // Declare dependencies
        AddDependency<TransformSystem>();            // Needs transform indices
        AddDependency<CameraSystem>();               // Needs camera for frustum culling
        AddDependency<BoundingBoxUpdateSystem>();    // Needs updated world AABBs
        AddDependency<RenderPrimitiveCollectSystem>(); // Needs collected items
    }

    void RenderPrimitiveBatchSystem::Update(float /*deltaTime*/)
    {
        if (!world || !cameraInfo)
            return;

        auto& cache = world->GetRenderFrameCache();
        if (cache.renderItems.empty())
            return;

        stats = Statistics{};
        stats.totalEntities = cache.renderItems.size();

        if (frustumCullingEnabled)
            PerformFrustumCulling();

        if (distanceSortingEnabled)
            SortByDistance();

        TransformSystem* transform_system = nullptr;
        if (auto system = world->GetSystem<TransformSystem>())
        {
            transform_system = system.get();
            transform_system->SetDevice(device);
            transform_system->EnsureTransformBuffer();
            transform_system->RefreshHandleOrder();
        }

        AssignTransformIndices(transform_system);

        if (batchingEnabled)
        {
            const auto start = std::chrono::high_resolution_clock::now();

            BuildMaterialBatches();
            FinalizeBatches();

            const auto end = std::chrono::high_resolution_clock::now();
            stats.batchingTimeMs = std::chrono::duration<float, std::milli>(end - start).count();
            stats.batchCount = cache.materialBatches.size();

            if (events.onBatchesBuilt)
                events.onBatchesBuilt(stats.batchCount);
            if (events.onBatchingComplete)
                events.onBatchingComplete();
        }
    }

    void RenderPrimitiveBatchSystem::PerformFrustumCulling()
    {
        auto& cache = world->GetRenderFrameCache();

        const auto start = std::chrono::high_resolution_clock::now();

        if (events.onCullingStart)
            events.onCullingStart(cache.renderItems.size());

        frustum.SetMatrix(cameraInfo->vp);

        for (auto& itemPtr : cache.renderItems)
        {
            PrimitiveRenderItem* item = itemPtr.get();
            if (!item || !item->isVisible)
                continue;

            auto entity = item->GetEntity();
            if (!entity)
                continue;

            auto bbox = entity->GetComponent<BoundingBoxComponent>();
            if (bbox)
            {
                if (bbox->HasWorldAABB())
                {
                    const auto& world_aabb = bbox->GetWorldAABB();
                    const glm::vec3 world_center = world_aabb.GetCenter();
                    const glm::vec3 world_extents = world_aabb.GetExtent();
                    const float radius = glm::length(world_extents);

                    item->isVisible = (frustum.SphereIn(world_center, radius) != math::Frustum::Scope::OUTSIDE);
                }
                else
                {
                    const glm::vec3 local_center = bbox->GetCenter();
                    const glm::vec3 local_extents = bbox->GetExtents();
                    const float radius = glm::length(local_extents);

                    const glm::mat4 worldMat = item->GetWorldMatrix();
                    const glm::vec3 world_center = glm::vec3(worldMat * glm::vec4(local_center, 1.0f));

                    item->isVisible = (frustum.SphereIn(world_center, radius) != math::Frustum::Scope::OUTSIDE);
                }
            }
            else
            {
                auto primitiveComp = item->GetPrimitiveComponent();
                if (!primitiveComp)
                    continue;

                auto transform = item->GetTransform();
                if (!transform)
                    continue;

                glm::vec3 worldPos = transform->GetWorldPosition();
                float boundingRadius = primitiveComp->GetBoundingRadius();

                if (boundingRadius <= 0.0f)
                    item->isVisible = true;
                else
                    item->isVisible = (frustum.SphereIn(worldPos, boundingRadius) != math::Frustum::Scope::OUTSIDE);
            }
        }

        size_t visible_count = 0;
        for (const auto& itemPtr : cache.renderItems)
        {
            if (itemPtr && itemPtr->isVisible)
                ++visible_count;
        }

        stats.visibleEntities = visible_count;
        stats.culledEntities = cache.renderItems.size() - visible_count;

        const auto end = std::chrono::high_resolution_clock::now();
        stats.cullingTimeMs = std::chrono::duration<float, std::milli>(end - start).count();

        if (events.onCullingComplete)
            events.onCullingComplete(stats.visibleEntities, stats.culledEntities);
    }

    void RenderPrimitiveBatchSystem::SortByDistance()
    {
        auto& cache = world->GetRenderFrameCache();

        const auto start = std::chrono::high_resolution_clock::now();

        std::sort(cache.renderItems.begin(), cache.renderItems.end(),
            [](const std::unique_ptr<PrimitiveRenderItem>& a, const std::unique_ptr<PrimitiveRenderItem>& b) {
                return a->distanceToCamera < b->distanceToCamera;
            });

        const auto end = std::chrono::high_resolution_clock::now();
        stats.sortingTimeMs = std::chrono::duration<float, std::milli>(end - start).count();

        if (events.onSortingComplete)
            events.onSortingComplete(cache.renderItems.size());
    }

    void RenderPrimitiveBatchSystem::AssignTransformIndices(TransformSystem* transform_system)
    {
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
                item->transform_index = transform->IsMovable() ? (dynamic_base + group_index) : group_index;
            }
            else
            {
                item->transform_index = 0;
            }
        }
    }

    void RenderPrimitiveBatchSystem::BuildMaterialBatches()
    {
        auto& cache = world->GetRenderFrameCache();

        ECSTransformAssignmentBuffer* shared_transform_buffer = nullptr;
        if (auto transform_system = world->GetSystem<TransformSystem>())
        {
            shared_transform_buffer = transform_system->GetTransformBuffer();
        }

        for (auto& itemPtr : cache.renderItems)
        {
            PrimitiveRenderItem* item = itemPtr.get();
            if (!item || !item->isVisible)
                continue;

            auto* material = item->GetMaterial();
            auto* pipeline = item->GetPipeline();

            if (!material || !pipeline)
                continue;

            MaterialPipelineKey key(material, pipeline);
            auto it = cache.materialBatches.find(key);

            if (it == cache.materialBatches.end())
            {
                auto batch = std::make_unique<MaterialBatch>(key, device);
                batch->cameraInfo = cameraInfo;
                batch->transform_buffer = shared_transform_buffer;
                batch->AddItem(item);
                cache.materialBatches[key] = std::move(batch);
            }
            else
            {
                it->second->transform_buffer = shared_transform_buffer;
                it->second->AddItem(item);
            }
        }
    }

    void RenderPrimitiveBatchSystem::FinalizeBatches()
    {
        auto& cache = world->GetRenderFrameCache();

        for (auto& pair : cache.materialBatches)
        {
            if (pair.second)
                FinalizeBatch(*pair.second);
        }
    }
}//namespace hgl::ecs

