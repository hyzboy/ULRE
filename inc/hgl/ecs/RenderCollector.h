#pragma once

#include<hgl/ecs/System.h>
#include<hgl/ecs/BoundingBoxComponent.h>
#include<hgl/ecs/RenderItem.h>
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
        class Material;
        class Pipeline;
    }
}

namespace hgl::ecs
{
    // Forward declarations
    class World;
    class PrimitiveComponent;

    /**
     * Material/Pipeline index for batching
     * Similar to hgl::graph::PipelineMaterialIndex
     */
    struct MaterialPipelineKey
    {
        hgl::graph::Material* material;
        hgl::graph::Pipeline* pipeline;

        MaterialPipelineKey(hgl::graph::Material* m = nullptr, hgl::graph::Pipeline* p = nullptr)
            : material(m), pipeline(p) {}

        bool operator<(const MaterialPipelineKey& other) const
        {
            if (material < other.material) return true;
            if (material > other.material) return false;
            return pipeline < other.pipeline;
        }

        bool operator==(const MaterialPipelineKey& other) const
        {
            return material == other.material && pipeline == other.pipeline;
        }
    };

    /**
     * Material batch - holds render items with same material/pipeline
     * Similar to hgl::graph::PipelineMaterialBatch
     */
    class MaterialBatch
    {
    private:
        MaterialPipelineKey key;
        std::vector<RenderItem*> items;
        const graph::CameraInfo* cameraInfo;

    public:
        MaterialBatch(const MaterialPipelineKey& k) : key(k), cameraInfo(nullptr) {}
        ~MaterialBatch() = default;

        void SetCameraInfo(const graph::CameraInfo* info) { cameraInfo = info; }
        void Clear() { items.clear(); }
        void AddItem(RenderItem* item);
        void Finalize();  // Sort and prepare for rendering
        
        const std::vector<RenderItem*>& GetItems() const { return items; }
        const MaterialPipelineKey& GetKey() const { return key; }
        size_t GetCount() const { return items.size(); }
    };

    /**
        * RenderCollector System
        * Collects entities with TransformComponent and RenderableComponent
        * Performs frustum culling, batching by material/pipeline, and distance sorting
        * 
        * Redesigned to support PrimitiveComponent with batching similar to
        * the old hgl::graph::RenderCollector system
        */
    class RenderCollector : public System
    {
    private:

        std::shared_ptr<World> world;
        const graph::CameraInfo* cameraInfo;  // Pointer to camera info from CMMath
        math::Frustum frustum;  // Frustum from CMMath
        
        // Render items storage - now uses polymorphic RenderItem pointers
        std::vector<std::unique_ptr<RenderItem>> renderItems;
        
        // Material batching support (similar to RenderBatchMap)
        std::map<MaterialPipelineKey, std::unique_ptr<MaterialBatch>> materialBatches;
        
        bool frustumCullingEnabled;
        bool distanceSortingEnabled;
        bool batchingEnabled;
        uint32_t renderableCount;  // Count of renderable items

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

        /// Enable/disable material batching
        void SetBatchingEnabled(bool enabled) { batchingEnabled = enabled; }

        /// Collect all renderable entities (builds render items and batches)
        void CollectRenderables();

        /// Get collected render items (raw list)
        const std::vector<std::unique_ptr<RenderItem>>& GetRenderItems() const { return renderItems; }

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

        /// Clear all collected data (similar to old RenderCollector::Clear)
        void Clear();

        /// Update transform data for all items
        void UpdateTransformData();

        /// Update material instance data for a specific component
        void UpdateMaterialInstanceData(PrimitiveComponent* comp);

    private:

        /// Perform frustum culling on render items
        void PerformFrustumCulling();

        /// Sort render items by distance to camera (near to far)
        void SortByDistance();

        /// Build material batches from render items
        void BuildMaterialBatches();

        /// Finalize batches (sort and prepare for rendering)
        void FinalizeBatches();
    };
}//namespace hgl::ecs
