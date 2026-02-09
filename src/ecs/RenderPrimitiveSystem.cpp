#include<hgl/ecs/RenderPrimitiveSystem.h>
#include<hgl/ecs/Context.h>
#include<hgl/ecs/BoundingBoxComponent.h>
#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/TransformSystem.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/pipeline/VKPipeline.h>
#include<iostream>

namespace hgl::ecs
{
    RenderPrimitiveSystem::RenderPrimitiveSystem(const std::string& name)
        : System(name)
        , cameraInfo(nullptr)
        , device(nullptr)
        , frustumCullingEnabled(true)
        , distanceSortingEnabled(true)
        , batchingEnabled(true)
        , renderableCount(0)
    {
    }

    void RenderPrimitiveSystem::Initialize()
    {
        renderItems.reserve(100); // Pre-allocate for performance
        SetInitialized(true);
    }

    void RenderPrimitiveSystem::Update(float deltaTime)
    {
        // Collect primitive renderables every frame
        CollectPrimitives();
        UpdateTransformData();
    }

    void RenderPrimitiveSystem::Render(graph::RenderCmdBuffer* cmdBuffer, float /*deltaTime*/)
    {
        RenderPrimitives(cmdBuffer);
    }

    void RenderPrimitiveSystem::Shutdown()
    {
        Clear();
    }

    void RenderPrimitiveSystem::SetCameraInfo(const graph::CameraInfo* info)
    {
        cameraInfo = info;

        // Extract frustum planes from camera info
        if (cameraInfo)
        {
            // Assuming CMMath's Frustum API: frustum.Set(*cameraInfo);
        }

        // Update camera info for all batches
        for (auto& pair : materialBatches)
        {
            pair.second->SetCameraInfo(cameraInfo);
        }
    }

    void RenderPrimitiveSystem::CollectPrimitives()
    {
        if (!world || !cameraInfo)
            return;

        renderItems.clear();
        renderableCount = 0;

        // Clear batches but keep allocated memory
        for (auto& pair : materialBatches)
        {
            pair.second->Clear();
        }

        std::vector<std::shared_ptr<PrimitiveComponent>> primitives;
        world->GetComponents<PrimitiveComponent>(primitives);

        for (const auto& primitiveComp : primitives)
        {
            if (!primitiveComp || !primitiveComp->IsVisible() || !primitiveComp->CanRender())
                continue;

            auto entity = primitiveComp->GetOwner();
            if(!entity)
                continue;

            auto transform = entity->GetComponent<TransformComponent>();
            if(!transform)
                continue;

            auto item = std::make_unique<PrimitiveRenderItem>(
                entity, transform, primitiveComp);

            glm::vec3 worldPos = transform->GetWorldPosition();
            item->worldPosition = worldPos;
            glm::vec3 cameraPos = glm::vec3(0.0f);
            glm::vec3 toCamera = worldPos - cameraPos;
            item->distanceToCamera = glm::length(toCamera);

            renderItems.push_back(std::move(item));
            renderableCount++;
        }

        if (frustumCullingEnabled)
        {
            PerformFrustumCulling();
        }

        if (distanceSortingEnabled)
        {
            SortByDistance();
        }

        if (auto transform_system = world->GetSystem<TransformSystem>())
        {
            transform_system->SetDevice(device);
            transform_system->EnsureTransformBuffer();
            transform_system->RefreshHandleOrder();
        }

        AssignTransformIndices();

        if (batchingEnabled)
        {
            BuildMaterialBatches();
            FinalizeBatches();
        }
    }

    void RenderPrimitiveSystem::PerformFrustumCulling()
    {
        if (!cameraInfo)
            return;

        frustum.SetMatrix(cameraInfo->vp);

        for (auto& itemPtr : renderItems)
        {
            PrimitiveRenderItem* item = itemPtr.get();
            if (!item->isVisible)
                continue;

            // Try to get BoundingBoxComponent first for a tighter sphere
            auto entity = item->GetEntity();
            if (!entity)
                continue;

            auto bbox = entity->GetComponent<BoundingBoxComponent>();

            if (bbox)
            {
                const glm::vec3 local_center = bbox->GetCenter();
                const glm::vec3 local_extents = bbox->GetExtents();
                const float radius = glm::length(local_extents);

                const glm::mat4 worldMat = item->GetWorldMatrix();
                const glm::vec3 world_center = glm::vec3(worldMat * glm::vec4(local_center, 1.0f));

                item->isVisible = (frustum.SphereIn(world_center, radius) != math::Frustum::Scope::OUTSIDE);
            }
            else
            {
                // Fall back to sphere culling using bounding radius
                auto primitiveComp = item->GetPrimitiveComponent();
                if (!primitiveComp)
                    continue;

                auto transform = item->GetTransform();
                if (!transform)
                    continue;

                glm::vec3 worldPos = transform->GetWorldPosition();
                float boundingRadius = primitiveComp->GetBoundingRadius();

                if (boundingRadius <= 0.0f)
                {
                    // No valid radius available; skip frustum culling for this item.
                    item->isVisible = true;
                }
                else
                {
                    item->isVisible = (frustum.SphereIn(worldPos, boundingRadius) != math::Frustum::Scope::OUTSIDE);
                }
            }
        }
    }

    void RenderPrimitiveSystem::SortByDistance()
    {
        // Sort render items by distance to camera (near to far)
        std::sort(renderItems.begin(), renderItems.end(),
            [](const std::unique_ptr<PrimitiveRenderItem>& a, const std::unique_ptr<PrimitiveRenderItem>& b) {
                return a->distanceToCamera < b->distanceToCamera;
            });
    }

    void RenderPrimitiveSystem::BuildMaterialBatches()
    {
        ECSTransformAssignmentBuffer* shared_transform_buffer = nullptr;

        if (auto transform_system = world ? world->GetSystem<TransformSystem>() : nullptr)
        {
            shared_transform_buffer = transform_system->GetTransformBuffer();
        }

        // Build material batches from visible render items
        for (auto& itemPtr : renderItems)
        {
            PrimitiveRenderItem* item = itemPtr.get();
            if (!item->isVisible)
                continue;

            // Get material and pipeline for batching
            auto* material = item->GetMaterial();
            auto* pipeline = item->GetPipeline();

            if (!material || !pipeline)
                continue;

            // Create or get batch for this material/pipeline combination
            MaterialPipelineKey key(material, pipeline);
            auto it = materialBatches.find(key);

            if (it == materialBatches.end())
            {
                // Create new batch with device
                auto batch = std::make_unique<MaterialBatch>(key, device);
                batch->SetCameraInfo(cameraInfo);
                batch->SetTransformBuffer(shared_transform_buffer);
                batch->AddItem(item);
                materialBatches[key] = std::move(batch);
            }
            else
            {
                // Add to existing batch
                it->second->SetTransformBuffer(shared_transform_buffer);
                it->second->AddItem(item);
            }
        }
    }

    void RenderPrimitiveSystem::AssignTransformIndices()
    {
        auto transform_system = world ? world->GetSystem<TransformSystem>() : nullptr;
        uint32_t static_count = 0;
        uint32_t dynamic_count = 0;
        uint32_t dynamic_base = 0;

        if (transform_system)
        {
            static_count = transform_system->GetStaticCount();
            dynamic_count = transform_system->GetDynamicCount();
            dynamic_base = transform_system->GetDynamicBaseIndex(static_count, dynamic_count);
        }

        for (auto& itemPtr : renderItems)
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

    void RenderPrimitiveSystem::FinalizeBatches()
    {
        // Finalize all batches (sort items within each batch)
        for (auto& pair : materialBatches)
        {
            pair.second->Finalize();
        }
    }

    void RenderPrimitiveSystem::Clear()
    {
        renderItems.clear();
        materialBatches.clear();
        renderableCount = 0;
    }

    void RenderPrimitiveSystem::UpdateTransformData()
    {
        // Update transform data for all render items
        for (auto& itemPtr : renderItems)
        {
            itemPtr->UpdateWorldMatrix();
        }
    }

    void RenderPrimitiveSystem::UpdateMaterialInstanceData(PrimitiveComponent* comp)
    {
        if (!comp || !comp->CanRender())
            return;

        // Find the batch for this component's material/pipeline
        auto* material = comp->GetMaterial();
        auto* pipeline = comp->GetPipeline();

        if (!material || !pipeline)
            return;

        MaterialPipelineKey key(material, pipeline);
        auto it = materialBatches.find(key);

        if (it != materialBatches.end())
        {
            // Batch found - material instance data needs to be updated
            // The actual update would be done during rendering
        }
    }

    void RenderPrimitiveSystem::RenderPrimitives(graph::RenderCmdBuffer* cmdBuffer)
    {
        if (!cmdBuffer)
        {
            std::cerr << "[RenderPrimitiveSystem::RenderPrimitives] ERROR: No command buffer!" << std::endl;
            return;
        }

        if (renderableCount == 0)
        {
            std::cerr << "[RenderPrimitiveSystem::RenderPrimitives] WARNING: No renderable items!" << std::endl;
            return;
        }

        // Render each material batch
        int batchIndex = 0;
        for (auto& pair : materialBatches)
        {
            MaterialBatch* batch = pair.second.get();

            if (batch)
            {
                if (batch->GetCount() > 0)
                {
                    batch->Render(cmdBuffer);
                }
                // else
                // {
                //     std::cerr << "[RenderPrimitiveSystem::RenderPrimitives]   Skipping empty batch" << std::endl;
                // }
            }
            // else
            // {
            //     std::cerr << "[RenderPrimitiveSystem::RenderPrimitives]   ERROR: Null batch pointer!" << std::endl;
            // }

            batchIndex++;
        }
    }
}//namespace hgl::ecs
