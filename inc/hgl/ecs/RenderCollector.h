#pragma once

#include<hgl/ecs/System.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
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
         * Camera information for rendering
         * Simplified version based on typical camera needs
         */
        struct CameraInfo
        {
            glm::mat4 viewMatrix;           // View matrix
            glm::mat4 projectionMatrix;     // Projection matrix
            glm::mat4 viewProjectionMatrix; // Combined VP matrix
            glm::vec3 position;             // Camera position in world space
            glm::vec3 forward;              // Camera forward direction
            float nearPlane;                // Near clipping plane
            float farPlane;                 // Far clipping plane
            float fov;                      // Field of view (degrees)

            CameraInfo()
                : viewMatrix(1.0f)
                , projectionMatrix(1.0f)
                , viewProjectionMatrix(1.0f)
                , position(0.0f, 0.0f, 0.0f)
                , forward(0.0f, 0.0f, -1.0f)
                , nearPlane(0.1f)
                , farPlane(1000.0f)
                , fov(45.0f)
            {
            }

            // Update combined matrix
            void UpdateViewProjection()
            {
                viewProjectionMatrix = projectionMatrix * viewMatrix;
            }
        };

        /**
         * Frustum for view frustum culling
         * Uses 6 planes for efficient culling
         */
        class Frustum
        {
        private:

            // Frustum planes: left, right, bottom, top, near, far
            glm::vec4 planes[6];

        public:

            // Extract frustum planes from view-projection matrix
            void ExtractFromMatrix(const glm::mat4& vpMatrix)
            {
                // Left plane
                planes[0] = glm::vec4(
                    vpMatrix[0][3] + vpMatrix[0][0],
                    vpMatrix[1][3] + vpMatrix[1][0],
                    vpMatrix[2][3] + vpMatrix[2][0],
                    vpMatrix[3][3] + vpMatrix[3][0]
                );

                // Right plane
                planes[1] = glm::vec4(
                    vpMatrix[0][3] - vpMatrix[0][0],
                    vpMatrix[1][3] - vpMatrix[1][0],
                    vpMatrix[2][3] - vpMatrix[2][0],
                    vpMatrix[3][3] - vpMatrix[3][0]
                );

                // Bottom plane
                planes[2] = glm::vec4(
                    vpMatrix[0][3] + vpMatrix[0][1],
                    vpMatrix[1][3] + vpMatrix[1][1],
                    vpMatrix[2][3] + vpMatrix[2][1],
                    vpMatrix[3][3] + vpMatrix[3][1]
                );

                // Top plane
                planes[3] = glm::vec4(
                    vpMatrix[0][3] - vpMatrix[0][1],
                    vpMatrix[1][3] - vpMatrix[1][1],
                    vpMatrix[2][3] - vpMatrix[2][1],
                    vpMatrix[3][3] - vpMatrix[3][1]
                );

                // Near plane
                planes[4] = glm::vec4(
                    vpMatrix[0][3] + vpMatrix[0][2],
                    vpMatrix[1][3] + vpMatrix[1][2],
                    vpMatrix[2][3] + vpMatrix[2][2],
                    vpMatrix[3][3] + vpMatrix[3][2]
                );

                // Far plane
                planes[5] = glm::vec4(
                    vpMatrix[0][3] - vpMatrix[0][2],
                    vpMatrix[1][3] - vpMatrix[1][2],
                    vpMatrix[2][3] - vpMatrix[2][2],
                    vpMatrix[3][3] - vpMatrix[3][2]
                );

                // Normalize planes
                for (int i = 0; i < 6; i++)
                {
                    float length = glm::length(glm::vec3(planes[i]));
                    planes[i] /= length;
                }
            }

            // Test if a point is inside the frustum
            bool ContainsPoint(const glm::vec3& point) const
            {
                for (int i = 0; i < 6; i++)
                {
                    if (glm::dot(glm::vec3(planes[i]), point) + planes[i].w < 0.0f)
                    {
                        return false;
                    }
                }
                return true;
            }

            // Test if a sphere is inside or intersects the frustum
            bool ContainsSphere(const glm::vec3& center, float radius) const
            {
                for (int i = 0; i < 6; i++)
                {
                    float distance = glm::dot(glm::vec3(planes[i]), center) + planes[i].w;
                    if (distance < -radius)
                    {
                        return false;
                    }
                }
                return true;
            }
        };

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
            CameraInfo cameraInfo;
            Frustum frustum;
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
            void SetCameraInfo(const CameraInfo& info);

            /// Enable/disable frustum culling
            void SetFrustumCullingEnabled(bool enabled) { frustumCullingEnabled = enabled; }

            /// Enable/disable distance sorting
            void SetDistanceSortingEnabled(bool enabled) { distanceSortingEnabled = enabled; }

            /// Collect all renderable entities
            void CollectRenderables();

            /// Get collected render items
            const std::vector<RenderItem>& GetRenderItems() const { return renderItems; }

            /// Get camera info
            const CameraInfo& GetCameraInfo() const { return cameraInfo; }

        private:

            /// Perform frustum culling on render items
            void PerformFrustumCulling();

            /// Sort render items by distance to camera (near to far)
            void SortByDistance();
        };
    }//namespace ecs
}//namespace hgl
