#pragma once

#include<hgl/ecs/core/ShaderProgramPipelineKey.h>
#include<hgl/ecs/support/PipelineMaterialRenderer.h>
#include<hgl/common/DescriptorSetTypeDef.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/vk/VK.h>
#include<vector>

namespace hgl
{
    namespace graph
    {
        class CameraInfo;
        class BufferManager;
        class VulkanDevice;
        class IndirectMeshTaskBuffer;
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
     * ShaderProgram batch - holds render items with same material/pipeline
     * Similar to hgl::graph::PipelineMaterialBatch
     *
     * Manages rendering of all items with the same material/pipeline combination
     * Supports both direct and indirect rendering
     */
    struct MaterialBatch
    {
    public:

        ShaderProgramPipelineKey                key;                                        ///<材质/管线键
        std::vector<RenderItem *>               items;                                      ///<渲染项列表
        uint32_t                                static_count            = 0;                ///<静态项数量
        const graph::CameraInfo *               cameraInfo              = nullptr;          ///<相机信息
        graph::VulkanDevice *                   device                  = nullptr;          ///<设备指针
        graph::BufferManager *                  buffer_manager          = nullptr;          ///<缓冲区管理器

        // IndirectMeshDraw：mesh shader 间接命令（{X=组数, Y=实例数, Z=1}）+ per-draw 参数表
        //（BuildBatches 与命令同序写行；直接绘制/私有 VBO 走参数表 offset 视图）
        graph::IndirectMeshTaskBuffer *          icb_mesh_tasks          = nullptr;          ///<mesh 间接命令缓冲
        graph::DeviceBuffer *                    mesh_draw_params_buffer = nullptr;          ///<mesh per-draw 参数表 SSBO（每 DrawBatch 一行）
        uint32_t                                 mesh_draw_params_capacity = 0;              ///<参数表容量（行数）

        // Per-batch L2W index rows SSBO — written in draw order so gl_InstanceIndex
        // directly maps to the correct L2W matrix slot.
        graph::DeviceBuffer *                   l2w_index_rows_buffer       = nullptr;      ///<每批 L2W 行表 SSBO（draw order）
        uint32_t                                l2w_index_rows_capacity     = 0;            ///<L2W 行表容量（元素数）

        // Per-batch DataIndex rows SSBO — each row carries one struct slot index per instance.
        // row[i].values[DefaultMaterialPrivateDataSlot] = data_index of items[i].
        graph::DeviceBuffer *                   material_data_index_rows_buffer   = nullptr;  ///<每批 DataIndex 行表 SSBO（draw order）
        uint32_t                                material_data_index_rows_capacity = 0;        ///<DataIndex 行表容量（元素数）

        TransformAssignmentBuffer *          transform_buffer        = nullptr;          ///<Transform分配缓冲(非拥有；由 TransformSystem 持有——系统销毁后此指针失效，勿跨帧缓存系统指针，A6)
        graph::MaterialParameters *           batch_descriptor_mp[graph::DESCRIPTOR_SET_TYPE_COUNT]{}; ///<批次级描述符参数（按 set type）
        bool                                  has_batch_descriptor_overrides = false;            ///<是否启用批次级描述符覆盖
        bool                                  descriptor_bind_valid = true;                      ///<批次级descriptor绑定是否有效

        DrawBatchArray                          draw_batches;                               ///<绘制批次数组
        uint32_t                                draw_batches_count      = 0;                ///<有效批次数量

        PipelineMaterialRenderer *           renderer                = nullptr;          ///<ECS渲染器实例

    public:

        MaterialBatch(const ShaderProgramPipelineKey& k, graph::VulkanDevice* dev = nullptr, graph::BufferManager* bm = nullptr);
        ~MaterialBatch();

        void Clear();
        void AddItem(RenderItem* item);
        bool HasMovableRange() const { return static_count < items.size(); }
    };
}//namespace hgl::ecs
