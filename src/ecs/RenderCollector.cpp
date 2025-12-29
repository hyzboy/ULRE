#include<hgl/ecs/RenderCollector.h>
#include<hgl/ecs/World.h>

namespace hgl
{
    namespace ecs
    {
        RenderCollector::RenderCollector(const std::string& name)
            : System(name)
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

        void RenderCollector::SetCameraInfo(const CameraInfo& info)
        {
            cameraInfo = info;
            
            // Extract frustum planes from view-projection matrix
            frustum.ExtractFromMatrix(cameraInfo.viewProjectionMatrix);
        }

        void RenderCollector::CollectRenderables()
        {
            if (!world)
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
                    glm::vec3 toCamera = worldPos - cameraInfo.position;
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
            for (auto& item : renderItems)
            {
                if (!item.isVisible)
                    continue;

                // Get world position
                glm::vec3 worldPos = item.transform->GetWorldPosition();

                // Get bounding radius from renderable component
                float boundingRadius = item.renderable->GetBoundingRadius();

                // Test against frustum using sphere test
                item.isVisible = frustum.ContainsSphere(worldPos, boundingRadius);
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
