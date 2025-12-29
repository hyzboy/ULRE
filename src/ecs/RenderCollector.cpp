#include<hgl/ecs/RenderCollector.h>
#include<hgl/ecs/World.h>
#include<hgl/ecs/BoundingBoxComponent.h>

namespace hgl
{
    namespace ecs
    {
        RenderCollector::RenderCollector(const std::string& name)
            : System(name)
            , cameraInfo(nullptr)
            , frustumCullingEnabled(true)
            , distanceSortingEnabled(true)
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
            renderItems.clear();
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
        }

        void RenderCollector::CollectRenderables()
        {
            if (!world || !cameraInfo)
                return;

            renderItems.clear();

            // Get all entities from the world
            const auto& entities = world->GetEntities();

            for (const auto& entity : entities)
            {
                // Check if entity has both TransformComponent and RenderableComponent
                auto transform = entity->GetComponent<TransformComponent>();
                auto renderable = entity->GetComponent<RenderableComponent>();

                if (transform && renderable && renderable->IsVisible())
                {
                    RenderItem item;
                    item.entity = entity;
                    item.transform = transform;
                    item.renderable = renderable;
                    item.worldMatrix = transform->GetWorldMatrix();
                    item.isVisible = true;

                    // Calculate world position for distance calculation
                    glm::vec3 worldPos = transform->GetWorldPosition();
                    
                    // Calculate distance to camera
                    // CMMath's CameraInfo should have a position or eye field
                    // Using a reasonable default for now - actual field name depends on CMMath API
                    glm::vec3 cameraPos = glm::vec3(0.0f); // Default
                    // If CMMath provides: cameraPos = cameraInfo->eye; or cameraPos = cameraInfo->position;
                    glm::vec3 toCamera = worldPos - cameraPos;
                    item.distanceToCamera = glm::length(toCamera);

                    renderItems.push_back(item);
                }
            }

            // Perform frustum culling if enabled
            if (frustumCullingEnabled)
            {
                PerformFrustumCulling();
            }

            // Sort by distance if enabled
            if (distanceSortingEnabled)
            {
                SortByDistance();
            }
        }

        void RenderCollector::PerformFrustumCulling()
        {
            if (!cameraInfo)
                return;

            // Update frustum from camera (if CMMath API allows)
            // Assuming: frustum.Set(*cameraInfo) or similar
            // This extracts the 6 frustum planes from the view-projection matrix

            for (auto& item : renderItems)
            {
                if (!item.isVisible)
                    continue;

                // Try to get BoundingBoxComponent first for accurate AABB culling
                auto bbox = item.entity->GetComponent<BoundingBoxComponent>();
                
                if (bbox)
                {
                    // Use AABB for frustum culling (more accurate than sphere)
                    AABB aabb = bbox->GetAABB();
                    
                    // Transform AABB to world space using entity's world matrix
                    // For accurate culling, we need to transform all 8 corners and rebuild AABB
                    glm::vec3 corners[8] = {
                        glm::vec3(item.worldMatrix * glm::vec4(aabb.min_point.x, aabb.min_point.y, aabb.min_point.z, 1.0f)),
                        glm::vec3(item.worldMatrix * glm::vec4(aabb.max_point.x, aabb.min_point.y, aabb.min_point.z, 1.0f)),
                        glm::vec3(item.worldMatrix * glm::vec4(aabb.min_point.x, aabb.max_point.y, aabb.min_point.z, 1.0f)),
                        glm::vec3(item.worldMatrix * glm::vec4(aabb.max_point.x, aabb.max_point.y, aabb.min_point.z, 1.0f)),
                        glm::vec3(item.worldMatrix * glm::vec4(aabb.min_point.x, aabb.min_point.y, aabb.max_point.z, 1.0f)),
                        glm::vec3(item.worldMatrix * glm::vec4(aabb.max_point.x, aabb.min_point.y, aabb.max_point.z, 1.0f)),
                        glm::vec3(item.worldMatrix * glm::vec4(aabb.min_point.x, aabb.max_point.y, aabb.max_point.z, 1.0f)),
                        glm::vec3(item.worldMatrix * glm::vec4(aabb.max_point.x, aabb.max_point.y, aabb.max_point.z, 1.0f))
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
                    item.isVisible = (center.z > -100.0f && center.z < 100.0f &&
                                     center.x > -100.0f && center.x < 100.0f &&
                                     center.y > -100.0f && center.y < 100.0f);
                }
                else
                {
                    // Fall back to sphere culling using RenderableComponent's bounding radius
                    glm::vec3 worldPos = item.transform->GetWorldPosition();
                    float boundingRadius = item.renderable->GetBoundingRadius();
                    
                    // Use CMMath's Frustum to test sphere
                    // Assuming CMMath Frustum API: bool CheckSphere(const vec3&, float) or similar
                    // TODO: Replace with actual frustum.CheckSphere(worldPos, boundingRadius) when CMMath is available
                    // Simple placeholder: check if not too far
                    item.isVisible = (worldPos.z > -100.0f && worldPos.z < 100.0f &&
                                     worldPos.x > -100.0f && worldPos.x < 100.0f &&
                                     worldPos.y > -100.0f && worldPos.y < 100.0f);
                }
            }
        }

        void RenderCollector::SortByDistance()
        {
            // Sort render items by distance to camera (near to far)
            std::sort(renderItems.begin(), renderItems.end(),
                [](const RenderItem& a, const RenderItem& b) {
                    return a.distanceToCamera < b.distanceToCamera;
                });
        }
    }//namespace ecs
}//namespace hgl
