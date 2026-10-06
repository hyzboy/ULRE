#pragma once

#include <hgl/ecs/support/RenderPassSlot.h>

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
        uint64_t                                world_addresses_address           = 0;        ///<本批次所属**世界表**（WorldAddresses）本帧槽的基址；pc_root 用

        // GPU-Driven 渲染管线覆盖支持
        bool                                     gpu_driven_override               = false;    ///<该批次是否为 GPU-Driven 托管（跳过 CPU 侧 ICB 和行表生成）

        // T3i（③ 多批）：本批在剔除输出**共享竞技场**里的行切片（gather 输出四表共用
        // 一套 arena，按批分摊）。语义：行 r 的 arena 索引 = pass 切片.row_base + r。
        // 着色器按此写；绘制侧把 cmds/l2w/mtl 的 push 地址各加 `row_base × 行字节`
        // ⇒ 着色器内 `gl_DrawID`/`gl_InstanceIndex` 语义不变（切片内 0 起）。
        //
        // T4c3c：切片**按 pass 存**（下标 = `RenderPassSlot`）。
        // 为什么必须按 pass：批对象**跨 pass 共享**（主帧与阴影复用同一张 `materialBatches`
        // 表），而两个 pass 各有自己的批块与竞技场扇区（`BatchBlockBase` / `ArenaSectorBase`）
        // ⇒ 只存一套的话，后接管的 pass 会**覆盖**前一个的切片 ⇒ 绘制侧拿别的 pass 的行基址
        // 画（实测：CascadeShadowMap 挂死）。
        // 注意 `gpu_driven_override` **保持单份**：它是"本帧装配是否被接管"的批级事实，
        // 批构建路径（sort/ensure/build/write 的十处跳过）读的就是它，与 pass 无关。
        struct GpuSlice
        {
            uint32_t row_base            = 0;   ///<切片起点（arena 行号）
            uint32_t capacity            = 0;   ///<切片容量（= 本批 item 数；绘制 maxDrawCount）
            uint32_t count_buffer_offset = 0;   ///<本 pass 的每批计数偏移（计数块按 pass 分块）
        };

        GpuSlice                                 gpu_slice[RenderPassSlot::Count];

        /// 本 pass 的切片（越界 pass 由调用点 fail-fast；这里不做钳位，下标直取）。
        const GpuSlice &GetGpuSlice(uint32_t pass_index) const { return gpu_slice[pass_index]; }
        bool                                     uses_render_item_resolve          = false;    ///<是否在 Shader 中通过 WorldAddresses::addr_global_render_items 进行 4-ID 运行时解析
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
