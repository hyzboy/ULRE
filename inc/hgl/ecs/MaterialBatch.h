#pragma once

#include<hgl/ecs/MaterialPipelineKey.h>
#include<vector>

namespace hgl
{
    namespace graph
    {
        class CameraInfo;
        class RenderCmdBuffer;
        class VulkanDevice;
    }
}

namespace hgl::ecs
{
    // Forward declaration
    class RenderItem;

    /**
     * Material batch - holds render items with same material/pipeline
     * Similar to hgl::graph::PipelineMaterialBatch
     * 
     * Manages rendering of all items with the same material/pipeline combination
     */
    class MaterialBatch
    {
    private:
        MaterialPipelineKey key;
        std::vector<RenderItem*> items;
        const graph::CameraInfo* cameraInfo;
        graph::VulkanDevice* device;

    public:
        MaterialBatch(const MaterialPipelineKey& k, graph::VulkanDevice* dev = nullptr);
        ~MaterialBatch() = default;

        void SetCameraInfo(const graph::CameraInfo* info) { cameraInfo = info; }
        void SetDevice(graph::VulkanDevice* dev) { device = dev; }
        
        void Clear() { items.clear(); }
        void AddItem(RenderItem* item);
        void Finalize();  // Sort and prepare for rendering
        
        /// Render all items in this batch
        void Render(graph::RenderCmdBuffer* cmdBuffer);
        
        const std::vector<RenderItem*>& GetItems() const { return items; }
        const MaterialPipelineKey& GetKey() const { return key; }
        size_t GetCount() const { return items.size(); }
    };
}//namespace hgl::ecs
