#include<hgl/ecs/systems/render/RenderPrimitiveBatchSystem.h>
#include<source_location>
#include<cstdio>
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
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/utils/ObjectTracker.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/vk/VKRenderAssign.h>
#include<hgl/vk/VKIndirectCommandBuffer.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
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
    }//anonymous namespace

    void RenderPrimitiveBatchSystem::ReallocICB(MaterialBatch& batch)
    {
        HGL_CAPTURE_SCOPE();
        if (!device || batch.items.empty())
        {
            std::cout << "[ECS::RenderPrimitiveBatchSystem] Cannot allocate ICB - Device: "
                        << (void*)device << ", Items: " << batch.items.size() << std::endl;
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

        // 构建带有上下文信息的名字
        graph::ObjectNameBuilder draw_name;
        graph::ObjectNameBuilder indexed_name;

        if (world && !world->GetResourceNamePrefix().empty())
        {
            // 从上下文获取前缀并追加类型信息
            std::string draw_str = world->GetResourceNamePrefix() + ":IndirectDrawBuffer";
            std::string indexed_str = world->GetResourceNamePrefix() + ":IndirectDrawIndexedBuffer";

            draw_name = graph::ObjectNameBuilder(draw_str.c_str());
            indexed_name = graph::ObjectNameBuilder(indexed_str.c_str());
        }
        else
        {
            // 默认名字：使用调用者位置追踪（当world为空时）
            const auto loc = std::source_location::current();
            const char* filename = loc.file_name();
            const char* basename = filename;
            for (const char* p = filename; *p; ++p)
                if (*p == '\\' || *p == '/') basename = p + 1;

            char draw_buf[64], indexed_buf[64];
            snprintf(draw_buf, sizeof(draw_buf), "ICB_Draw@%s:%u", basename, loc.line());
            snprintf(indexed_buf, sizeof(indexed_buf), "ICB_DrawIdx@%s:%u", basename, loc.line());

            draw_name = graph::ObjectNameBuilder(draw_buf);
            indexed_name = graph::ObjectNameBuilder(indexed_buf);
        }

        batch.icb_draw = device->CreateIndirectDrawBuffer(icb_new_count, draw_name);
        batch.icb_draw_indexed = device->CreateIndirectDrawIndexedBuffer(icb_new_count, indexed_name);
    }

    void RenderPrimitiveBatchSystem::BuildBatches(MaterialBatch& batch, const uint32_t base_instance)
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

    void RenderPrimitiveBatchSystem::FinalizeBatch(MaterialBatch& batch)
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

        BuildBatches(batch, 0);

        if (batch.key.material && batch.key.material->hasMI())
        {
            if (!batch.mi_buffer && !batch.items.empty())
                batch.mi_buffer = new ECSMaterialInstanceAssignmentBuffer(batch.buffer_manager, batch.key.material);

            if (batch.mi_buffer && !batch.items.empty())
                batch.mi_buffer->WriteItems(batch.items);
        }

        if (batch.buffer_manager && !batch.items.empty())
        {
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
                auto render_ctx = world ? world->GetRenderContext() : nullptr;
                auto graphics_context = render_ctx ? render_ctx->GetGraphicsContext() : nullptr;
                if (!graphics_context && world)
                    graphics_context = world->GetGraphicsContext();
                auto buffer_manager = graphics_context ? graphics_context->GetBufferManager() : nullptr;
                auto batch = std::make_unique<MaterialBatch>(key, device, buffer_manager);
                batch->cameraInfo = cameraInfo;
                batch->transform_buffer = shared_transform_buffer;
                batch->AddItem(item);
                cache.materialBatches[key] = std::move(batch);
            }
            else
            {
                auto render_ctx = world ? world->GetRenderContext() : nullptr;
                auto graphics_context = render_ctx ? render_ctx->GetGraphicsContext() : nullptr;
                if (!graphics_context && world)
                    graphics_context = world->GetGraphicsContext();
                auto buffer_manager = graphics_context ? graphics_context->GetBufferManager() : nullptr;
                it->second->buffer_manager = buffer_manager;
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

