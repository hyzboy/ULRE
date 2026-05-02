#pragma once

#include <cstdint>
#include <limits>
#include <utility>
#include <unordered_map>

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
        struct ResolvedPipelineCacheKey
        {
            graph::Primitive* primitive = nullptr;
            const graph::RenderTargetFormat* render_format = nullptr;

            bool operator==(const ResolvedPipelineCacheKey &rhs) const noexcept
            {
                return primitive == rhs.primitive
                    && render_format == rhs.render_format;
            }
        };

        struct ResolvedPipelineCacheKeyHash
        {
            size_t operator()(const ResolvedPipelineCacheKey &k) const noexcept
            {
                const size_t h1 = std::hash<graph::Primitive*>{}(k.primitive);
                const size_t h2 = std::hash<const graph::RenderTargetFormat*>{}(k.render_format);
                return h1 ^ (h2 << 1);
            }
        };

        ECSContext* world = nullptr;
        const graph::CameraInfo* camera_info = nullptr;
        graph::VulkanDevice* device = nullptr;
        math::Frustum frustum;
        uint32_t prepared_frame_index = std::numeric_limits<uint32_t>::max();
        std::unordered_map<ResolvedPipelineCacheKey, graph::GraphicsPipeline*, ResolvedPipelineCacheKeyHash> resolved_pipeline_cache;

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
        void BuildMaterialBatches();
        void FinalizeBatches();

        graph::BufferManager* GetBufferManager() const;
        std::pair<graph::ObjectNameBuilder, graph::ObjectNameBuilder> BuildICBNames() const;

        void ReallocICB(MaterialBatch& batch);
        void BuildBatches(MaterialBatch& batch, const uint32_t base_instance);
        void FinalizeBatch(MaterialBatch& batch);
        void SortBatchItems(MaterialBatch& batch);
        void UpdateMaterialInstanceBuffer(MaterialBatch& batch);
    };
}
