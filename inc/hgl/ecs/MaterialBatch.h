#pragma once

#include<hgl/ecs/MaterialPipelineKey.h>
#include<hgl/graph/VK.h>
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
        uint32_t static_count = 0;
        const graph::CameraInfo* cameraInfo;
        graph::VulkanDevice* device;

        // === Indirect rendering support ===
        graph::IndirectDrawBuffer* icb_draw=nullptr;               ///<间接绘制命令缓冲（无索引）
        graph::IndirectDrawIndexedBuffer* icb_draw_indexed=nullptr;///<间接绘制命令缓冲（有索引）

        graph::VAB* transform_vab=nullptr;                        ///<Transform index VAB (per batch)
        VkBuffer transform_vab_buffer=VK_NULL_HANDLE;             ///<Transform index VAB buffer
        uint32_t transform_vab_node_count=0;                      ///<Transform VAB capacity

        // === Instance data management (shared) ===
        ECSTransformAssignmentBuffer* transform_buffer=nullptr;           ///<Transform分配缓冲(非拥有)
        ECSMaterialInstanceAssignmentBuffer* mi_buffer=nullptr;           ///<材质实例分配缓冲

        // === Draw batches ===
        graph::DrawBatchArray draw_batches;         ///<绘制批次数组
        uint32_t draw_batches_count=0;              ///<有效批次数量

        // === Renderer (ECS version) ===
        ECSPipelineMaterialRenderer* renderer;             ///<ECS渲染器实例

        // === Batch building helper methods ===
        void ReallocICB(const std::vector<RenderItem*>& list,
            graph::IndirectDrawBuffer*& icb_draw_out,
            graph::IndirectDrawIndexedBuffer*& icb_draw_indexed_out);     ///<重新分配间接绘制缓冲

        void WriteICB(VkDrawIndirectCommand*, graph::DrawBatch*);
        void WriteICB(VkDrawIndexedIndirectCommand*, graph::DrawBatch*);
        void BuildBatches(const std::vector<RenderItem*>& list,
                  graph::DrawBatchArray& batches,
                  uint32_t& batch_count,
                  graph::IndirectDrawBuffer*& icb_draw_out,
                  graph::IndirectDrawIndexedBuffer*& icb_draw_indexed_out,
                  const uint32_t base_instance);  ///<构建绘制批次和间接命令


    public:
        MaterialBatch(const MaterialPipelineKey& k, graph::VulkanDevice* dev = nullptr);
        ~MaterialBatch();

        void SetCameraInfo(const graph::CameraInfo* info) { cameraInfo = info; }
        void SetDevice(graph::VulkanDevice* dev) { device = dev; }
        void SetTransformBuffer(ECSTransformAssignmentBuffer* buf) { transform_buffer = buf; }

        void Clear()
        {
            items.clear();
            static_count = 0;
            draw_batches.clear();
            draw_batches_count = 0;
        }
        void AddItem(RenderItem* item);
        void Finalize();  // Sort and prepare for rendering (builds indirect commands)

        /// Render all items in this batch
        void Render(graph::RenderCmdBuffer* cmdBuffer);

        const std::vector<RenderItem*>& GetItems() const { return items; }
        const MaterialPipelineKey& GetKey() const { return key; }
        size_t GetCount() const { return items.size(); }
        bool HasMovableRange() const { return static_count < items.size(); }
        void QueueMovableTransformUpdates();
    };
}//namespace hgl::ecs
