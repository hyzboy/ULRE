#pragma once

#include<hgl/ecs/System.h>
#include<hgl/math/geometry/Frustum.h>

namespace hgl
{
    namespace graph
    {
        class CameraInfo;
        class VulkanDevice;
    }
}

namespace hgl::ecs
{
    class ECSContext;
    class TransformSystem;

    /**
     * RenderPrimitiveBatchSystem
     *
     * Performs frustum culling, sorting, transform index assignment,
     * and material batching for primitive render items.
     */
    class RenderPrimitiveBatchSystem : public System
    {
    private:

        ECSContext* world = nullptr;
        const graph::CameraInfo* cameraInfo = nullptr;
        graph::VulkanDevice* device = nullptr;
        math::Frustum frustum;

        bool frustumCullingEnabled = true;
        bool distanceSortingEnabled = true;
        bool batchingEnabled = true;

    public:

        RenderPrimitiveBatchSystem(const std::string& name = "RenderPrimitiveBatchSystem");
        ~RenderPrimitiveBatchSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }
        void SetCameraInfo(const graph::CameraInfo* info) { cameraInfo = info; }
        void SetDevice(graph::VulkanDevice* dev) { device = dev; }

        void SetFrustumCullingEnabled(bool enabled) { frustumCullingEnabled = enabled; }
        void SetDistanceSortingEnabled(bool enabled) { distanceSortingEnabled = enabled; }
        void SetBatchingEnabled(bool enabled) { batchingEnabled = enabled; }

        void Update(float deltaTime) override;

    private:

        void PerformFrustumCulling();
        void SortByDistance();
        void AssignTransformIndices(TransformSystem* transform_system);
        void BuildMaterialBatches();
        void FinalizeBatches();
    };
}//namespace hgl::ecs
