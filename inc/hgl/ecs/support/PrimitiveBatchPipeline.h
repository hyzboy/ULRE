#pragma once

#include <cstdint>
#include <limits>
#include <utility>

#include<hgl/math/geometry/Frustum.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/vk/VKObjectNameBuilder.h>

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
    class RenderItem;
    class PrimitiveRenderItem;
    class BoundingBoxComponent;

    class PrimitiveBatchPipeline
    {
        OBJECT_LOGGER

    private:
        ECSContext* world = nullptr;
        const graph::CameraInfo* camera_info = nullptr;
        graph::VulkanDevice* device = nullptr;
        math::Frustum frustum;
        uint32_t prepared_frame_index = std::numeric_limits<uint32_t>::max();

    public:
        bool PrepareFrame(ECSContext* ctx);

        void RunCulling();
        void RunSorting();
        void RunTransformIndexing();
        void RunBatching();

    private:
        void PerformFrustumCulling();

        bool TestFrustumWithWorldAABB(RenderItem* item, const BoundingBoxComponent* bbox);
        bool TestFrustumWithLocalAABB(RenderItem* item, const BoundingBoxComponent* bbox);
        bool TestFrustumWithBoundingSphere(RenderItem* item);

        void SortByDistance();
        void AssignTransformIndices(TransformSystem* transform_system);
        void BuildMaterialBatches();
        void FinalizeBatches();

        graph::BufferManager* GetBufferManager() const;
        std::pair<graph::ObjectNameBuilder, graph::ObjectNameBuilder> BuildICBNames() const;

        void ReallocICB(MaterialBatch& batch);
        void BuildBatches(MaterialBatch& batch, const uint32_t base_instance);
        void FinalizeBatch(MaterialBatch& batch);
        void SortBatchItems(MaterialBatch& batch);
        void UpdateMaterialInstanceBuffer(MaterialBatch& batch);
        void EnsureTransformVAB(MaterialBatch& batch);
        void WriteTransformIndices(MaterialBatch& batch);
    };
}
