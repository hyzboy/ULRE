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

    void RenderPrimitiveSystem::Render(graph::RenderCmdBuffer* /*cmdBuffer*/, float /*deltaTime*/)
    {
        // Rendering is handled by RenderPrimitiveRenderSystem.
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

        // Update frustum from camera (if CMMath API allows)
        // Assuming: frustum.Set(*cameraInfo) or similar

        for (auto& itemPtr : renderItems)
        {
            PrimitiveRenderItem* item = itemPtr.get();
            if (!item->isVisible)
                continue;

            // Try to get BoundingBoxComponent first for accurate AABB culling
            auto entity = item->GetEntity();
            if (!entity)
                continue;

            auto bbox = entity->GetComponent<BoundingBoxComponent>();

            if (bbox)
            {
                // Use AABB for frustum culling (more accurate than sphere)
                AABB aabb = bbox->GetAABB();

                // Transform AABB to world space using entity's world matrix
                glm::mat4 worldMat = item->GetWorldMatrix();

                // For accurate culling, transform all 8 corners and rebuild AABB
                glm::vec3 corners[8] = {
                    glm::vec3(worldMat * glm::vec4(aabb.GetMin().x, aabb.GetMin().y, aabb.GetMin().z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(aabb.GetMax().x, aabb.GetMin().y, aabb.GetMin().z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(aabb.GetMin().x, aabb.GetMax().y, aabb.GetMin().z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(aabb.GetMax().x, aabb.GetMax().y, aabb.GetMin().z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(aabb.GetMin().x, aabb.GetMin().y, aabb.GetMax().z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(aabb.GetMax().x, aabb.GetMin().y, aabb.GetMax().z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(aabb.GetMin().x, aabb.GetMax().y, aabb.GetMax().z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(aabb.GetMax().x, aabb.GetMax().y, aabb.GetMax().z, 1.0f))
                };

                // Find transformed AABB bounds
                glm::vec3 transformedMin = corners[0];
                glm::vec3 transformedMax = corners[0];
                for (int i = 1; i < 8; ++i)
                {
                    transformedMin = glm::min(transformedMin, corners[i]);
                    transformedMax = glm::max(transformedMax, corners[i]);
                }

                AABB worldAABB(transformedMin, transformedMax);

                // Use CMMath's Frustum to test AABB
                // TODO: Replace with actual frustum.CheckAABB(worldAABB) when CMMath is available
                glm::vec3 center = (transformedMin + transformedMax) * 0.5f;
                item->isVisible = (center.z > -100.0f && center.z < 100.0f &&
                                    center.x > -100.0f && center.x < 100.0f &&
                                    center.y > -100.0f && center.y < 100.0f);
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

                // Use CMMath's Frustum to test sphere
                // TODO: Replace with actual frustum.CheckSphere(worldPos, boundingRadius)
                item->isVisible = (worldPos.z > -100.0f && worldPos.z < 100.0f &&
                                    worldPos.x > -100.0f && worldPos.x < 100.0f &&
                                    worldPos.y > -100.0f && worldPos.y < 100.0f);
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
        auto static_storage = TransformComponent::GetStaticStorage();
        auto dynamic_storage = TransformComponent::GetDynamicStorage();

        const uint32_t static_count = static_cast<uint32_t>(static_storage ? static_storage->GetSize() : 0);
        const uint32_t dynamic_count = static_cast<uint32_t>(dynamic_storage ? dynamic_storage->GetSize() : 0);

        uint32_t dynamic_base = static_count;

        if (auto transform_system = world ? world->GetSystem<TransformSystem>() : nullptr)
        {
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

            if (transform->IsMovable())
                item->transform_index = dynamic_base + handle;
            else
                item->transform_index = handle;
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
