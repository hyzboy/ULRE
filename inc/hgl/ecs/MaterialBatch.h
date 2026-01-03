#pragma once

#include<hgl/ecs/MaterialPipelineKey.h>
#include<hgl/graph/PipelineMaterialRenderer.h>
#include<vector>

namespace hgl
{
    namespace graph
    {
        class CameraInfo;
        class RenderCmdBuffer;
        class VulkanDevice;
        class IndirectDrawBuffer;
        class IndirectDrawIndexedBuffer;
    }
    
    namespace ecs
    {
        class ECSTransformAssignmentBuffer;
        class ECSMaterialInstanceAssignmentBuffer;
        class ECSPipelineMaterialRenderer;
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
     * Supports both direct and indirect rendering
     */
    class MaterialBatch
    {
    private:
        MaterialPipelineKey key;
        std::vector<RenderItem*> items;
        const graph::CameraInfo* cameraInfo;
        graph::VulkanDevice* device;

        // === Indirect rendering support ===
        graph::IndirectDrawBuffer* icb_draw;               ///<间接绘制命令缓冲（无索引）
        graph::IndirectDrawIndexedBuffer* icb_draw_indexed;///<间接绘制命令缓冲（有索引）
        
        // === Instance data management (ECS versions) ===
        ECSTransformAssignmentBuffer* transform_buffer;             ///<Transform分配缓冲（ECS版本）
        ECSMaterialInstanceAssignmentBuffer* mi_buffer;             ///<材质实例分配缓冲（ECS版本）
        
        // === Draw batches ===
        graph::DrawBatchArray draw_batches;                ///<绘制批次数组
        uint32_t draw_batches_count;                       ///<有效批次数量
        
        // === Renderer (ECS version) ===
        ECSPipelineMaterialRenderer* renderer;             ///<ECS渲染器实例

        // === Batch building helper methods ===
        void ReallocICB();                          ///<重新分配间接绘制缓冲
        void WriteICB(VkDrawIndirectCommand*, graph::DrawBatch*);
        void WriteICB(VkDrawIndexedIndirectCommand*, graph::DrawBatch*);
        void BuildBatches();                        ///<构建绘制批次和间接命令

    public:
        MaterialBatch(const MaterialPipelineKey& k, graph::VulkanDevice* dev = nullptr);
        ~MaterialBatch();

        void SetCameraInfo(const graph::CameraInfo* info) { cameraInfo = info; }
        void SetDevice(graph::VulkanDevice* dev) { device = dev; }
        
        void Clear() { items.clear(); }
        void AddItem(RenderItem* item);
        void Finalize();  // Sort and prepare for rendering (builds indirect commands)
        
        /// Render all items in this batch
        void Render(graph::RenderCmdBuffer* cmdBuffer);
        
        const std::vector<RenderItem*>& GetItems() const { return items; }
        const MaterialPipelineKey& GetKey() const { return key; }
        size_t GetCount() const { return items.size(); }
    };
}//namespace hgl::ecs
