#pragma once

#include<hgl/ecs/System.h>
#include<hgl/ecs/BoundingBoxComponent.h>
#include<hgl/ecs/RenderItem.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/math/geometry/Frustum.h>
#include<vector>
#include<memory>
#include<algorithm>

namespace hgl::ecs
{
    // Forward declarations
    class World;

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
}//namespace hgl::ecs
