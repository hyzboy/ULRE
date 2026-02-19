#pragma once

#include<hgl/ecs/core/MaterialPipelineKey.h>
#include<hgl/ecs/support/PipelineMaterialRenderer.h>
#include<hgl/vk/VK.h>
#include<vector>

namespace hgl
{
    namespace graph
    {
        class CameraInfo;
        class BufferManager;
        class VulkanDevice;
        class IndirectDrawBuffer;
        class IndirectDrawIndexedBuffer;
    }

    namespace ecs
    {
        class TransformAssignmentBuffer;
        class MaterialInstanceAssignmentBuffer;
        class PipelineMaterialRenderer;
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
    struct MaterialBatch
    {
    public:

        MaterialPipelineKey                     key;                                        ///<材质/管线键
        std::vector<RenderItem *>               items;                                      ///<渲染项列表
        uint32_t                                static_count            = 0;                ///<静态项数量
        const graph::CameraInfo *               cameraInfo              = nullptr;          ///<相机信息
        graph::VulkanDevice *                   device                  = nullptr;          ///<设备指针
        graph::BufferManager *                  buffer_manager          = nullptr;          ///<缓冲区管理器

        graph::IndirectDrawBuffer *             icb_draw                = nullptr;          ///<间接绘制命令缓冲（无索引）
        graph::IndirectDrawIndexedBuffer *      icb_draw_indexed        = nullptr;          ///<间接绘制命令缓冲（有索引）

        graph::VAB *                            transform_vab           = nullptr;          ///<Transform索引VAB
        VkBuffer                                transform_vab_buffer    = VK_NULL_HANDLE;   ///<Transform索引VAB缓冲
        uint32_t                                transform_vab_node_count= 0;                ///<Transform VAB容量

        TransformAssignmentBuffer *          transform_buffer        = nullptr;          ///<Transform分配缓冲(非拥有)
        MaterialInstanceAssignmentBuffer *   mi_buffer               = nullptr;          ///<材质实例分配缓冲

        DrawBatchArray                          draw_batches;                               ///<绘制批次数组
        uint32_t                                draw_batches_count      = 0;                ///<有效批次数量

        PipelineMaterialRenderer *           renderer                = nullptr;          ///<ECS渲染器实例

    public:

        MaterialBatch(const MaterialPipelineKey& k, graph::VulkanDevice* dev = nullptr, graph::BufferManager* bm = nullptr);
        ~MaterialBatch();

        void Clear()
        {
            items.clear();
            static_count = 0;
            draw_batches.clear();
            draw_batches_count = 0;
        }
        void AddItem(RenderItem* item);
        bool HasMovableRange() const { return static_count < items.size(); }
    };
}//namespace hgl::ecs

