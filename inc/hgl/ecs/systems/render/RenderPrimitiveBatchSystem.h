#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/math/geometry/Frustum.h>
#include<functional>
#include<cstddef>

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

        struct Statistics
        {
            size_t totalEntities = 0;
            size_t visibleEntities = 0;
            size_t culledEntities = 0;
            size_t batchCount = 0;
            float cullingTimeMs = 0.0f;
            float sortingTimeMs = 0.0f;
            float batchingTimeMs = 0.0f;
        } stats;

    public:

        struct Events
        {
            std::function<void(size_t totalEntities)> onCullingStart;
            std::function<void(size_t visibleCount, size_t culledCount)> onCullingComplete;
            std::function<void(size_t totalEntities)> onSortingComplete;
            std::function<void(size_t batchCount)> onBatchesBuilt;
            std::function<void()> onBatchingComplete;
        } events;

        RenderPrimitiveBatchSystem(const std::string& name = "RenderPrimitiveBatchSystem");
        ~RenderPrimitiveBatchSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }
        void SetCameraInfo(const graph::CameraInfo* info) { cameraInfo = info; }
        void SetDevice(graph::VulkanDevice* dev) { device = dev; }

        const graph::CameraInfo* GetCameraInfo() const { return cameraInfo; }
        graph::VulkanDevice* GetDevice() const { return device; }

        void SetFrustumCullingEnabled(bool enabled) { frustumCullingEnabled = enabled; }
        void SetDistanceSortingEnabled(bool enabled) { distanceSortingEnabled = enabled; }
        void SetBatchingEnabled(bool enabled) { batchingEnabled = enabled; }

        const Statistics& GetStatistics() const { return stats; }

        void Update(float deltaTime) override;

    private:

        void PerformFrustumCulling();
        void SortByDistance();
        void AssignTransformIndices(TransformSystem* transform_system);
        void BuildMaterialBatches();
        void FinalizeBatches();
    };
}//namespace hgl::ecs

