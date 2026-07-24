#pragma once

#include<hgl/ecs/core/MaterialPipelineKey.h>
#include<hgl/ecs/support/PipelineMaterialRenderer.h>
#include<hgl/common/DescriptorSetTypeDef.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/vk/VK.h>
#include<array>
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
        class MaterialParameters;
    }

    namespace ecs
    {
        class TransformAssignmentBuffer;
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

        // Per-batch L2W index rows SSBO — written in draw order so gl_InstanceIndex
        // directly maps to the correct L2W matrix slot.
        graph::DeviceBuffer *                   l2w_index_rows_buffer       = nullptr;      ///<每批 L2W 行表 SSBO（draw order）
        uint32_t                                l2w_index_rows_capacity     = 0;            ///<L2W 行表容量（元素数）

        // Per-batch DataIndex rows SSBO — each row carries one DataSlot struct_index per instance.
        // row[i].values[DataSlot::PBRSurface] = MaterialInstance::GetMIID() of items[i].
        graph::DeviceBuffer *                   mi_data_index_rows_buffer   = nullptr;      ///<每批 DataIndex 行表 SSBO（draw order）
        uint32_t                                mi_data_index_rows_capacity = 0;            ///<DataIndex 行表容量（元素数）

        TransformAssignmentBuffer *          transform_buffer        = nullptr;          ///<Transform分配缓冲(非拥有)
        std::array<uint32_t, static_cast<size_t>(graph::mtl::TextureSlot::RANGE_SIZE)> texture_slot_handles{}; ///<每材质纹理槽的 bindless handle（用于实例纹理行表）
        bool                                  has_texture_slot_handles = false;
        graph::MaterialParameters *           batch_descriptor_mp[graph::DESCRIPTOR_SET_TYPE_COUNT]{}; ///<批次级描述符参数（按 set type）
        bool                                  has_batch_descriptor_overrides = false;            ///<是否启用批次级描述符覆盖

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
            texture_slot_handles.fill(0);
            has_texture_slot_handles = false;
        }
        void AddItem(RenderItem* item);
        bool HasMovableRange() const { return static_count < items.size(); }
    };
}//namespace hgl::ecs
