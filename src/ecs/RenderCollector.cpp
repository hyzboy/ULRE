#include<hgl/ecs/RenderCollector.h>
#include<hgl/ecs/World.h>
#include<hgl/ecs/BoundingBoxComponent.h>
#include<hgl/ecs/RenderableComponent.h>
#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/PrimitiveRenderItem.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/pipeline/VKPipeline.h>

namespace hgl::ecs
{
    // RenderCollector implementation
    RenderCollector::RenderCollector(const std::string& name)
        : System(name)
        , cameraInfo(nullptr)
        , device(nullptr)
        , frustumCullingEnabled(true)
        , distanceSortingEnabled(true)
        , batchingEnabled(true)
        , renderableCount(0)
    {
    }

    void RenderCollector::Initialize()
    {
        renderItems.reserve(100); // Pre-allocate for performance
        SetInitialized(true);
    }

    void RenderCollector::Update(float deltaTime)
    {
        // Collect renderables every frame
        CollectRenderables();
    }

    void RenderCollector::Shutdown()
    {
        Clear();
    }

    void RenderCollector::SetCameraInfo(const graph::CameraInfo* info)
    {
        cameraInfo = info;
            
        // Extract frustum planes from camera info
        // CMMath's Frustum should have a Set() or similar method that takes CameraInfo
        if (cameraInfo)
        {
            // Assuming CMMath's Frustum API: frustum.Set(*cameraInfo);
            // This would extract the 6 frustum planes from the view-projection matrix
            // For now, we'll call it in PerformFrustumCulling if needed
        }

        // Update camera info for all batches
        for (auto& pair : materialBatches)
        {
            pair.second->SetCameraInfo(cameraInfo);
        }
    }

    void RenderCollector::CollectRenderables()
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

        // Get all entities from the world
        const auto& entities = world->GetEntities();

        for (const auto& entity : entities)
        {
            // Check if entity has both TransformComponent and RenderableComponent
            auto transform = entity->GetComponent<TransformComponent>();
            auto renderable = entity->GetComponent<RenderableComponent>();

            if (transform && renderable && renderable->IsVisible())
            {
                // Check if it's a PrimitiveComponent (for batching support)
                auto primitiveComp = std::dynamic_pointer_cast<PrimitiveComponent>(renderable);
                
                if (primitiveComp && primitiveComp->CanRender())
                {
                    // Create PrimitiveRenderItem
                    auto item = std::make_unique<PrimitiveRenderItem>(
                        entity, transform, primitiveComp);
                    
                    // Calculate world position for distance calculation
                    glm::vec3 worldPos = transform->GetWorldPosition();
                    item->worldPosition = worldPos;
                    
                    // Calculate distance to camera
                    // CMMath's CameraInfo should have a position or eye field
                    // Using a reasonable default for now - actual field name depends on CMMath API
                    glm::vec3 cameraPos = glm::vec3(0.0f); // Default
                    // If CMMath provides: cameraPos = cameraInfo->eye; or cameraPos = cameraInfo->position;
                    glm::vec3 toCamera = worldPos - cameraPos;
                    item->distanceToCamera = glm::length(toCamera);

                    renderItems.push_back(std::move(item));
                    renderableCount++;
                }
                else if (renderable)
                {
                    // For other renderable types, we could create a generic RenderItem
                    // but for now, only PrimitiveComponent is supported for batching
                    // This could be extended in the future
                }
            }
        }

        // Perform frustum culling if enabled
        if (frustumCullingEnabled)
        {
            PerformFrustumCulling();
        }

        // Sort by distance if enabled (before batching)
        if (distanceSortingEnabled)
        {
            SortByDistance();
        }

        // Build material batches if enabled
        if (batchingEnabled)
        {
            BuildMaterialBatches();
            FinalizeBatches();
        }
    }

    void RenderCollector::PerformFrustumCulling()
    {
        if (!cameraInfo)
            return;

        // Update frustum from camera (if CMMath API allows)
        // Assuming: frustum.Set(*cameraInfo) or similar
        // This extracts the 6 frustum planes from the view-projection matrix

        for (auto& itemPtr : renderItems)
        {
            RenderItem* item = itemPtr.get();
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
                
                // For accurate culling, we need to transform all 8 corners and rebuild AABB
                glm::vec3 corners[8] = {
                    glm::vec3(worldMat * glm::vec4(aabb.min_point.x, aabb.min_point.y, aabb.min_point.z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(aabb.max_point.x, aabb.min_point.y, aabb.min_point.z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(aabb.min_point.x, aabb.max_point.y, aabb.min_point.z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(aabb.max_point.x, aabb.max_point.y, aabb.min_point.z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(aabb.min_point.x, aabb.min_point.y, aabb.max_point.z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(aabb.max_point.x, aabb.min_point.y, aabb.max_point.z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(aabb.min_point.x, aabb.max_point.y, aabb.max_point.z, 1.0f)),
                    glm::vec3(worldMat * glm::vec4(aabb.max_point.x, aabb.max_point.y, aabb.max_point.z, 1.0f))
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
                // Assuming CMMath Frustum API: bool CheckAABB(const AABB&) or similar
                // For now, use a placeholder that checks if center is roughly in view
                glm::vec3 center = (transformedMin + transformedMax) * 0.5f;
                // TODO: Replace with actual frustum.CheckAABB(worldAABB) when CMMath is available
                // Simple placeholder: check if not too far (z < -100)
                item->isVisible = (center.z > -100.0f && center.z < 100.0f &&
                                    center.x > -100.0f && center.x < 100.0f &&
                                    center.y > -100.0f && center.y < 100.0f);
            }
            else
            {
                // Fall back to sphere culling using RenderableComponent's bounding radius
                auto renderable = item->GetRenderable();
                if (!renderable)
                    continue;

                auto transform = item->GetTransform();
                if (!transform)
                    continue;

                glm::vec3 worldPos = transform->GetWorldPosition();
                float boundingRadius = renderable->GetBoundingRadius();
                    
                // Use CMMath's Frustum to test sphere
                // Assuming CMMath Frustum API: bool CheckSphere(const vec3&, float) or similar
                // TODO: Replace with actual frustum.CheckSphere(worldPos, boundingRadius) when CMMath is available
                // Simple placeholder: check if not too far
                item->isVisible = (worldPos.z > -100.0f && worldPos.z < 100.0f &&
                                    worldPos.x > -100.0f && worldPos.x < 100.0f &&
                                    worldPos.y > -100.0f && worldPos.y < 100.0f);
            }
        }
    }

    void RenderCollector::SortByDistance()
    {
        // Sort render items by distance to camera (near to far)
        std::sort(renderItems.begin(), renderItems.end(),
            [](const std::unique_ptr<RenderItem>& a, const std::unique_ptr<RenderItem>& b) {
                return a->distanceToCamera < b->distanceToCamera;
            });
    }

    void RenderCollector::BuildMaterialBatches()
    {
        // Build material batches from visible render items
        for (auto& itemPtr : renderItems)
        {
            RenderItem* item = itemPtr.get();
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
                batch->AddItem(item);
                materialBatches[key] = std::move(batch);
            }
            else
            {
                // Add to existing batch
                it->second->AddItem(item);
            }
        }
    }

    void RenderCollector::FinalizeBatches()
    {
        // Finalize all batches (sort items within each batch)
        for (auto& pair : materialBatches)
        {
            pair.second->Finalize();
        }
    }

    void RenderCollector::Clear()
    {
        renderItems.clear();
        materialBatches.clear();
        renderableCount = 0;
    }

    void RenderCollector::UpdateTransformData()
    {
        // Update transform data for all render items
        for (auto& itemPtr : renderItems)
        {
            // For PrimitiveRenderItem, update world matrix
            if (auto* primItem = dynamic_cast<PrimitiveRenderItem*>(itemPtr.get()))
            {
                primItem->UpdateWorldMatrix();
            }
        }
    }

    void RenderCollector::UpdateMaterialInstanceData(PrimitiveComponent* comp)
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
            // This is a placeholder for future implementation
        }
    }

    void RenderCollector::Render(graph::RenderCmdBuffer* cmdBuffer)
    {
        if (!cmdBuffer)
            return;

        if (renderableCount == 0)
            return;

        // Render each material batch
        for (auto& pair : materialBatches)
        {
            MaterialBatch* batch = pair.second.get();
            if (batch && batch->GetCount() > 0)
            {
                batch->Render(cmdBuffer);
            }
        }
    }
}//namespace hgl::ecs
