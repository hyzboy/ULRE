/**
 * ECSPipelineMaterialRenderer.h - ECS Pipeline材质渲染器
 * 
 * 参照 PipelineMaterialRenderer 设计，但支持 ECS 版本的 Assignment Buffers
 * 职责：
 * - 执行ECS渲染命令
 * - 管理渲染状态（VAB绑定、IBO绑定等）
 * - 处理直接绘制和间接绘制
 * - 与 ECSTransformAssignmentBuffer 和 ECSMaterialInstanceAssignmentBuffer 配合
 */

#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/PipelineMaterialRenderer.h>

namespace hgl
{
    namespace graph
    {
        class Material;
        class Pipeline;
        class RenderCmdBuffer;
        class VABList;
        class IndirectDrawBuffer;
        class IndirectDrawIndexedBuffer;
        struct GeometryDataBuffer;
        struct GeometryDrawRange;
    }
    
    namespace ecs
    {
        class ECSTransformAssignmentBuffer;
        class ECSMaterialInstanceAssignmentBuffer;
    }
}

namespace hgl::ecs
{
    /**
     * ECS Pipeline材质渲染器
     * 
     * 与 PipelineMaterialRenderer 功能相同，但接受 ECS 版本的 Assignment Buffers
     */
    class ECSPipelineMaterialRenderer
    {
    private:
        // === 核心标识 ===
        graph::Material* material;                          ///<材质
        graph::Pipeline* pipeline;                          ///<管线

        // === 渲染状态缓存 ===
        graph::RenderCmdBuffer* cmd_buf;                    ///<当前渲染命令缓冲
        graph::VABList* vab_list;                           ///<顶点属性缓冲列表

        const graph::GeometryDataBuffer* last_data_buffer;  ///<上次绑定的几何数据缓冲
        const graph::VDM* last_vdm;                         ///<上次使用的顶点数据管理器
        const graph::GeometryDrawRange* last_draw_range;    ///<上次使用的绘制范围

        int first_indirect_draw_index;                      ///<首个间接绘制索引
        uint32_t indirect_draw_count;                       ///<累积的间接绘制数量

        // === 渲染辅助方法 ===
        
        /**
         * 绑定顶点属性缓冲
         * @param batch 绘制批次
         * @param transform_buffer ECS Transform分配缓冲
         * @param mi_buffer ECS 材质实例分配缓冲
         * @return 绑定是否成功
         */
        bool BindVAB(const graph::DrawBatch* batch, 
                     ECSTransformAssignmentBuffer* transform_buffer, 
                     ECSMaterialInstanceAssignmentBuffer* mi_buffer);
        
        /**
         * 处理间接渲染
         * @param icb_draw 间接绘制缓冲（无索引）
         * @param icb_draw_indexed 间接绘制缓冲（有索引）
         */
        void ProcIndirectRender(graph::IndirectDrawBuffer* icb_draw, 
                               graph::IndirectDrawIndexedBuffer* icb_draw_indexed);
        
        /**
         * 绘制单个批次
         * @param batch 绘制批次
         * @param transform_buffer ECS Transform分配缓冲
         * @param mi_buffer ECS 材质实例分配缓冲
         * @param icb_draw 间接绘制缓冲（无索引）
         * @param icb_draw_indexed 间接绘制缓冲（有索引）
         * @return 绘制是否成功
         */
        bool Draw(graph::DrawBatch* batch,
                  ECSTransformAssignmentBuffer* transform_buffer,
                  ECSMaterialInstanceAssignmentBuffer* mi_buffer,
                  graph::IndirectDrawBuffer* icb_draw,
                  graph::IndirectDrawIndexedBuffer* icb_draw_indexed);

    public:
        ECSPipelineMaterialRenderer(graph::Material* m, graph::Pipeline* p);
        ~ECSPipelineMaterialRenderer();

        /**
         * 执行渲染
         * @param rcb 渲染命令缓冲
         * @param batches 绘制批次数组
         * @param batch_count 批次数量
         * @param transform_buffer ECS Transform分配缓冲（可为空）
         * @param mi_buffer ECS 材质实例分配缓冲（可为空）
         * @param icb_draw 间接绘制缓冲（无索引）
         * @param icb_draw_indexed 间接绘制缓冲（有索引）
         */
        void Render(graph::RenderCmdBuffer* rcb,
                    const graph::DrawBatchArray& batches,
                    uint32_t batch_count,
                    ECSTransformAssignmentBuffer* transform_buffer,
                    ECSMaterialInstanceAssignmentBuffer* mi_buffer,
                    graph::IndirectDrawBuffer* icb_draw,
                    graph::IndirectDrawIndexedBuffer* icb_draw_indexed);
    };
}//namespace hgl::ecs
