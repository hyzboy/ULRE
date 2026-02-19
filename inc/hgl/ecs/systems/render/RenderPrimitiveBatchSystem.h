#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/math/geometry/Frustum.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/ecs/support/PipelineMaterialRenderer.h>
#include<functional>
#include<cstddef>

namespace hgl
{
    namespace graph
    {
        class CameraInfo;
        class VulkanDevice;
        class BufferManager;
    }
}

namespace hgl::ecs
{
    class ECSContext;
    class TransformSystem;
    class PrimitiveRenderItem;
    class BoundingBoxComponent;

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

        // Frustum culling strategies
        bool TestFrustumWithWorldAABB(PrimitiveRenderItem* item, const BoundingBoxComponent* bbox);
        bool TestFrustumWithLocalAABB(PrimitiveRenderItem* item, const BoundingBoxComponent* bbox);
        bool TestFrustumWithBoundingSphere(PrimitiveRenderItem* item);

        void SortByDistance();
        void AssignTransformIndices(TransformSystem* transform_system);
        void BuildMaterialBatches();
        void FinalizeBatches();

        // Utility functions
        graph::BufferManager* GetBufferManager() const;
        std::pair<graph::ObjectNameBuilder, graph::ObjectNameBuilder> BuildICBNames() const;

        // Helper functions for batching
        void ReallocICB(MaterialBatch& batch);

        void BuildBatches(MaterialBatch& batch, const uint32_t base_instance);

        void FinalizeBatch(MaterialBatch& batch);

        // FinalizeBatch sub-functions
        void SortBatchItems(MaterialBatch& batch);
        void UpdateMaterialInstanceBuffer(MaterialBatch& batch);
        void EnsureTransformVAB(MaterialBatch& batch);
        void WriteTransformIndices(MaterialBatch& batch);
    };
}//namespace hgl::ecs

