#pragma once

#include<hgl/ecs/System.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/BoundingBoxComponent.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/math/geometry/Frustum.h>
#include<vector>
#include<memory>
#include<algorithm>

namespace hgl
{
    namespace ecs
    {
        // Forward declarations
        class World;
        class RenderableComponent;

        /**
         * Render item - collected entity with rendering data
         */
        struct RenderItem
        {
            std::shared_ptr<Entity> entity;
            std::shared_ptr<TransformComponent> transform;
            std::shared_ptr<RenderableComponent> renderable;
            glm::mat4 worldMatrix;
            float distanceToCamera;
            bool isVisible;

            RenderItem()
                : worldMatrix(1.0f)
                , distanceToCamera(0.0f)
                , isVisible(true)
            {
            }
        };

        /**
         * Base renderable component interface
         * Derived classes should implement specific rendering needs
         */
        class RenderableComponent : public Component
        {
        protected:

            bool visible;
            float boundingRadius; // Simple bounding sphere for frustum culling

        public:

            explicit RenderableComponent(const std::string& name = "Renderable")
                : Component(name)
                , visible(true)
                , boundingRadius(1.0f)
            {
            }

            virtual ~RenderableComponent() = default;

            bool IsVisible() const { return visible; }
            void SetVisible(bool v) { visible = v; }

            float GetBoundingRadius() const { return boundingRadius; }
            void SetBoundingRadius(float radius) { boundingRadius = radius; }

            // Override in derived classes for specific rendering
            virtual void Render(const glm::mat4& worldMatrix) {}
        };

        /**
         * RenderCollector System
         * Collects entities with TransformComponent and RenderableComponent
         * Performs frustum culling and distance sorting
         */
        class RenderCollector : public System
        {
        private:

            std::shared_ptr<World> world;
            const graph::CameraInfo* cameraInfo;  // Pointer to camera info from CMMath
            math::Frustum frustum;  // Frustum from CMMath
            std::vector<RenderItem> renderItems;
            bool frustumCullingEnabled;
            bool distanceSortingEnabled;

        public:

            RenderCollector(const std::string& name = "RenderCollector");
            ~RenderCollector() override = default;

        public:

            void Initialize() override;
            void Update(float deltaTime) override;
            void Shutdown() override;

        public:

            /// Set the world to collect from
            void SetWorld(std::shared_ptr<World> w) { world = w; }

            /// Set camera information
            void SetCameraInfo(const graph::CameraInfo* info);

            /// Enable/disable frustum culling
            void SetFrustumCullingEnabled(bool enabled) { frustumCullingEnabled = enabled; }

            /// Enable/disable distance sorting
            void SetDistanceSortingEnabled(bool enabled) { distanceSortingEnabled = enabled; }

            /// Collect all renderable entities
            void CollectRenderables();

            /// Get collected render items
            const std::vector<RenderItem>& GetRenderItems() const { return renderItems; }

            /// Get camera info
            const graph::CameraInfo* GetCameraInfo() const { return cameraInfo; }

        private:

            /// Perform frustum culling on render items
            void PerformFrustumCulling();

            /// Sort render items by distance to camera (near to far)
            void SortByDistance();
        };
    }//namespace ecs
}//namespace hgl
