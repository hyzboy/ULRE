/**
 * PipelineMaterialRenderer.h - ECS Pipeline材质渲染器
 *
 * 参照 PipelineMaterialRenderer 设计，但支持 ECS 版本的 Assignment Buffers
 * 职责：
 * - 执行ECS渲染命令
 * - 管理渲染状态（VAB绑定、IBO绑定等）
 * - 处理直接绘制和间接绘制
 * - 与 TransformAssignmentBuffer 配合
 */

#pragma once

#include<hgl/vk/VK.h>

namespace hgl
{
    namespace graph
    {
        class RenderContext;
        class ShaderProgram;
        class Pipeline;
        class RenderCmdBuffer;
        class VABList;
        class IndirectDrawBuffer;
        struct GeometryDataBuffer;
        struct GeometryDrawRange;
        class MaterialParameters;
    }

    namespace ecs
    {
        class TransformAssignmentBuffer;
    }
}

namespace hgl::ecs
{
    struct MaterialBatch;

    /**
     * 绘制批次：将使用相同几何数据的节点合并为一个批次
     */
    struct DrawBatch
    {
                uint32_t                first_instance = 0;     ///<第一个绘制实例(和instance渲染无关,对应InstanceRate的VAB)
                uint32_t                instance_count = 0;     ///<此批次包含的实例数量

        const   graph::GeometryDataBuffer *    geom_data_buffer = nullptr;   ///<几何数据缓冲
        const   graph::GeometryDrawRange *     geom_draw_range = nullptr;    ///<绘制范围（顶点/索引偏移和数量）
        const   graph::Geometry *              geometry = nullptr;           ///<几何体（per-DrawBatch 顶点 SSBO 绑定用——VAB 直取）

        graph::MaterialParameters *            per_object_mp = nullptr;      ///<per-DrawBatch 独立 PerObject set（descriptor set 是状态非快照——多对象独立 buffer 时共享单 set 提交时刻全用最后一次内容）

        void Set(const graph::GeometryDataBuffer *data_buffer,
                 const graph::GeometryDrawRange *draw_range,
                 const graph::Geometry *geom = nullptr)
        {
            geom_data_buffer = data_buffer;
            geom_draw_range = draw_range;
            geometry = geom;
        }
    };//struct DrawBatch

    using DrawBatchArray = std::vector<DrawBatch>;

    /**
     * ECS Pipeline材质渲染器
     *
     * 与 PipelineMaterialRenderer 功能相同，但接受 ECS 版本的 Assignment Buffers
     */
    class PipelineMaterialRenderer
    {
    private:
        // === 核心标识 ===
        graph::ShaderProgram* material;                          ///<材质
        graph::Pipeline* pipeline;                          ///<管线

        // === 渲染状态缓存 ===
        graph::RenderCmdBuffer* cmd_buf;                    ///<当前渲染命令缓冲

        const graph::GeometryDataBuffer* last_data_buffer;  ///<上次绑定的几何数据缓冲

        // === per-DrawBatch 独立 PerObject MP 池（descriptor set 是状态非快照：
        // 多对象独立 buffer 时，共享单 set 被 per-draw 顺序更新，提交时刻所有 draw
        // 读到最后一次更新的内容——每 draw 独立 set 解决） ===
        std::vector<graph::MaterialParameters*> per_object_mp_pool;   ///<per-draw PerObject MP 池（跨帧复用）

        // === SSBO 顶点输入缓存（per-DrawBatch 顶点 SSBO 绑定——buffer 未变跳过） ===
        bool ssbo_vertex_input = false;                     ///<材质是否走 SSBO 顶点输入
        VkBuffer last_ssbo_pos = VK_NULL_HANDLE;            ///<上次绑定的顶点 Position buffer
        VkBuffer last_ssbo_uv  = VK_NULL_HANDLE;            ///<上次绑定的顶点 UV buffer
        VkBuffer last_ssbo_ntb = VK_NULL_HANDLE;            ///<上次绑定的顶点 NTB buffer
        VkBuffer last_ssbo_color = VK_NULL_HANDLE;          ///<上次绑定的顶点 Color buffer
        VkBuffer last_ssbo_luminance = VK_NULL_HANDLE;      ///<上次绑定的顶点 Luminance buffer
        VkBuffer last_ssbo_transform_id = VK_NULL_HANDLE;   ///<上次绑定的顶点 TransformID buffer
        VkBuffer last_ssbo_size = VK_NULL_HANDLE;           ///<上次绑定的顶点 Size/宽度 buffer
        VkBuffer last_ssbo_index = VK_NULL_HANDLE;          ///<上次绑定的顶点索引 buffer

        int first_indirect_draw_index;                      ///<首个间接绘制索引
        uint32_t indirect_draw_count;                       ///<累积的间接绘制数量
        uint32_t indirect_draw_command_offset = 0;          ///<本批次已提交的间接命令数（ICB 命令序号累计）

        // === 渲染辅助方法 ===

        /**
         * 绑定顶点属性缓冲
         * @param batch 绘制批次
         * @return 绑定是否成功
         */
        bool BindVAB(const DrawBatch* batch);   // VBO 时代残留——已随阶段 4 删除实现

        /**
         * 处理间接渲染
         * @param icb_draw 间接绘制缓冲（SSBO 顶点输入统一非索引间接）
         */
        void ProcIndirectRender(graph::IndirectDrawBuffer* icb_draw);

        /**
         * 绘制单个批次
         * @param batch 绘制批次
         * @param transform_buffer ECS Transform分配缓冲
         * @param icb_draw 间接绘制缓冲（SSBO 顶点输入统一非索引间接）
         * @return 绘制是否成功
         */
        bool Draw(DrawBatch* batch,
                  TransformAssignmentBuffer* transform_buffer,
                  graph::IndirectDrawBuffer* icb_draw,
                  const MaterialBatch *owner_batch = nullptr);

    public:
        PipelineMaterialRenderer(graph::ShaderProgram* m, graph::Pipeline* p);
        ~PipelineMaterialRenderer();

        /**
         * 执行渲染
         * @param rcb 渲染命令缓冲
         * @param batches 绘制批次数组
         * @param batch_count 批次数量
         * @param transform_buffer ECS Transform分配缓冲（可为空）
         * @param icb_draw 间接绘制缓冲（无索引）
         * @param icb_draw_indexed 间接绘制缓冲（有索引）
         */
        void Render(graph::RenderCmdBuffer* rcb,
                    const DrawBatchArray& batches,
                    uint32_t batch_count,
                    TransformAssignmentBuffer* transform_buffer,
                    graph::IndirectDrawBuffer* icb_draw,
                    const MaterialBatch *owner_batch = nullptr,
                    graph::RenderContext *render_context = nullptr);
    };
}//namespace hgl::ecs
