#pragma once

#include<hgl/ecs/core/ShaderProgramPipelineKey.h>
#include<hgl/ecs/support/PipelineMaterialRenderer.h>
#include<hgl/common/DescriptorSetTypeDef.h>
#include<hgl/graph/ShaderBufferSources.h>
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
        graph::VulkanDevice *                   device                  = nullptr;          ///<设备指针
        graph::BufferManager *                  buffer_manager          = nullptr;          ///<缓冲区管理器

        // IndirectMeshDraw：mesh shader 间接命令（{X=组数, Y=实例数, Z=1}）+ per-draw 参数表
        //（BuildBatches 与命令同序写行；直接绘制/私有 VBO 走参数表 offset 视图）
        graph::IndirectMeshTaskBuffer *          icb_mesh_tasks          = nullptr;          ///<mesh 间接命令缓冲
        graph::DeviceBuffer *                    icb_count_buffer        = nullptr;          ///<mesh 间接绘制计数缓冲（可选，GPU-Driven 动态计数）
        VkDeviceSize                             icb_count_buffer_offset = 0;                ///<计数缓冲偏移
        graph::DeviceBuffer *                    mesh_draw_params_buffer = nullptr;          ///<mesh per-draw 参数表 SSBO（每 DrawBatch 一行）
        uint32_t                                 mesh_draw_params_capacity = 0;              ///<参数表容量（行数）

        // Per-batch L2W index rows SSBO — written in draw order so gl_InstanceIndex
        // directly maps to the correct L2W matrix slot.
        graph::DeviceBuffer *                   l2w_index_buffer        = nullptr;      ///<每批 L2W 索引表 SSBO（draw order）
        uint32_t                                l2w_index_capacity      = 0;            ///<L2W 索引表容量（元素数）

        // Per-batch material instance address rows SSBO — each row carries the
        // payload and texture-reference index for one draw item.
        graph::DeviceBuffer *                   material_data_index_rows_buffer   = nullptr;  ///<每批 DataIndex 行表 SSBO（draw order）
        uint32_t                                material_data_index_rows_capacity = 0;        ///<DataIndex 行表容量（元素数）
        uint64_t                                texture_reference_base_addr       = 0;        ///<当前材质对应的 MaterialTextureReferencePool GPU 基址

        // GPU-Driven 渲染管线覆盖支持
        bool                                     gpu_driven_override               = false;    ///<该批次是否为 GPU-Driven 托管（跳过 CPU 侧 ICB 和行表生成）
        bool                                     uses_render_item_resolve          = false;    ///<是否在 Shader 中通过 GlobalAddresses::addr_global_render_items 进行 4-ID 运行时解析
        bool                                     own_icb_mesh_tasks                = true;     ///<是否拥有 icb_mesh_tasks
        bool                                     own_mesh_draw_params              = true;     ///<是否拥有 mesh_draw_params_buffer
        bool                                     own_l2w_index                     = true;     ///<是否拥有 l2w_index_buffer
        bool                                     own_material_data_rows            = true;     ///<是否拥有 material_data_index_rows_buffer
        graph::DeviceBuffer *                    l2w_buffer                        = nullptr;  ///<显式指定的 L2W 矩阵缓冲（用于 GPU-Driven 大规模渲染）

        TransformAssignmentBuffer *          transform_buffer        = nullptr;          ///<Transform分配缓冲(非拥有；由 TransformSystem 持有——系统销毁后此指针失效，勿跨帧缓存系统指针，A6)

        bool                                  debug_blocks_logged = false;                       ///<[ArenaDebug] 首次行表诊断日志已输出                      ///<批次级descriptor绑定是否有效

        DrawBatchArray                          draw_batches;                               ///<绘制批次数组
        uint32_t                                draw_batches_count      = 0;                ///<有效批次数量

        PipelineMaterialRenderer *           renderer                = nullptr;          ///<ECS渲染器实例

    public:

        MaterialBatch(const ShaderProgramPipelineKey& k, graph::VulkanDevice* dev = nullptr, graph::BufferManager* bm = nullptr);
        ~MaterialBatch();

        void Clear();
        void AddItem(RenderItem* item);
    };
}//namespace hgl::ecs
