#include<hgl/ecs/RenderCollector.h>
#include<hgl/ecs/World.h>

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
            
            // Extract frustum planes from view-projection matrix when camera is set
            // Note: Exact API depends on CMMath's Frustum implementation
            // TODO: Update this when CMMath is available to use actual Frustum API
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
                    // TODO: Update to use CMMath CameraInfo's position field
                    // glm::vec3 toCamera = worldPos - cameraInfo->position;
                    // item.distanceToCamera = glm::length(toCamera);
                    item.distanceToCamera = glm::length(worldPos);  // Temporary placeholder

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
            // TODO: Implement using CMMath's Frustum class
            // For now, all items remain visible
            /*
            for (auto& item : renderItems)
            {
                if (!item.isVisible)
                    continue;

                glm::vec3 worldPos = item.transform->GetWorldPosition();
                float boundingRadius = item.renderable->GetBoundingRadius();
                
                // Use Frustum from CMMath
                item.isVisible = frustum.ContainsSphere(worldPos, boundingRadius);
            }
            */
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
