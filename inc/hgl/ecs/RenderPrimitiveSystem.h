#pragma once

#include<hgl/ecs/System.h>
#include<hgl/ecs/BoundingBoxComponent.h>
#include<hgl/ecs/PrimitiveRenderItem.h>
#include<hgl/ecs/MaterialPipelineKey.h>
#include<hgl/ecs/MaterialBatch.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/math/geometry/Frustum.h>
#include<vector>
#include<memory>
#include<map>
#include<algorithm>

namespace hgl
{
    namespace graph
    {
        class VulkanDevice;
        class RenderCmdBuffer;
    }
}

namespace hgl::ecs
{
    // Forward declarations
    class ECSContext;
    class PrimitiveComponent;

    /**
     * RenderPrimitiveSystem
     * 
     * Specialized rendering system for PrimitiveComponent entities.
     * Follows ECS design principle: one system per component type.
     * 
     * Features:
     * - Collects entities with TransformComponent and PrimitiveComponent
     * - Performs frustum culling using AABB or sphere
     * - Batches primitives by material/pipeline for efficient rendering
     * - Sorts by distance for optimal draw order
     * 
     * Future systems can handle other component types:
     * - RenderParticleSystem for particle effects
     * - RenderLineSystem for line/wire rendering
     * - RenderTerrainSystem for terrain chunks
     * etc.
     */
    class RenderPrimitiveSystem : public System
    {
    private:

        ECSContext* world = nullptr;
        const graph::CameraInfo* cameraInfo;
        graph::VulkanDevice* device;
        math::Frustum frustum;
        
        // Primitive-specific render items
        std::vector<std::unique_ptr<PrimitiveRenderItem>> renderItems;
        
        // Material batching for primitives
        std::map<MaterialPipelineKey, std::unique_ptr<MaterialBatch>> materialBatches;
        
        bool frustumCullingEnabled;
        bool distanceSortingEnabled;
        bool batchingEnabled;
        uint32_t renderableCount;

    public:

        RenderPrimitiveSystem(const std::string& name = "RenderPrimitiveSystem");
        ~RenderPrimitiveSystem() override = default;

    public:

        void Initialize() override;
        void Update(float deltaTime) override;
        void Render(graph::RenderCmdBuffer* cmdBuffer, float deltaTime) override;
        void Shutdown() override;

    public:

        /// Set the world to collect from
        void SetWorld(ECSContext* w) { world = w; }

        /// Set Vulkan device
        void SetDevice(graph::VulkanDevice* dev) { device = dev; }

        /// Set camera information
        void SetCameraInfo(const graph::CameraInfo* info);

        /// Enable/disable frustum culling
        void SetFrustumCullingEnabled(bool enabled) { frustumCullingEnabled = enabled; }

        /// Enable/disable distance sorting
        void SetDistanceSortingEnabled(bool enabled) { distanceSortingEnabled = enabled; }

        /// Enable/disable material batching
        void SetBatchingEnabled(bool enabled) { batchingEnabled = enabled; }

        /// Collect all primitive entities
        void CollectPrimitives();

        /// Get collected primitive render items
        const std::vector<std::unique_ptr<PrimitiveRenderItem>>& GetRenderItems() const 
        { 
            return renderItems; 
        }

        /// Get material batches (for efficient rendering)
        const std::map<MaterialPipelineKey, std::unique_ptr<MaterialBatch>>& GetMaterialBatches() const 
        { 
            return materialBatches; 
        }

        /// Get camera info
        const graph::CameraInfo* GetCameraInfo() const { return cameraInfo; }

        /// Get renderable count
        uint32_t GetRenderableCount() const { return renderableCount; }

        /// Check if empty
        bool IsEmpty() const { return renderableCount == 0; }

        /// Clear all collected data
        void Clear();

        /// Update transform data for all items
        void UpdateTransformData();

        /// Update material instance data for a specific component
        void UpdateMaterialInstanceData(PrimitiveComponent* comp);

        /// Render all collected primitives
        /// Similar to hgl::graph::PipelineMaterialBatch::Render
        void RenderPrimitives(graph::RenderCmdBuffer* cmdBuffer);

    private:

        /// Perform frustum culling on render items
        void PerformFrustumCulling();

        /// Sort render items by distance to camera
        void SortByDistance();

        /// Build material batches from render items
        void BuildMaterialBatches();

        /// Finalize batches (sort and prepare for rendering)
        void FinalizeBatches();
    };
}//namespace hgl::ecs
