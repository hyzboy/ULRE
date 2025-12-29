#pragma once

#include<hgl/ecs/MaterialPipelineKey.h>
#include<vector>

namespace hgl
{
    namespace graph
    {
        class CameraInfo;
    }
}

namespace hgl::ecs
{
    // Forward declaration
    class RenderItem;

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
}//namespace hgl::ecs
