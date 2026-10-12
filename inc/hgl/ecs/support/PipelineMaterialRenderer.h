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

#include <hgl/ecs/support/RenderPassSlot.h>

#include<hgl/vk/VK.h>
#include <hgl/ecs/core/RenderItem.h>
#include<hgl/mtl/MeshShaderLimits.h>
#include<hgl/graph/geo/VKGeometry.h>

namespace hgl
{
    namespace graph
    {
        class RenderContext;
        class ShaderProgram;
        class Pipeline;
        class RenderCmdBuffer;
        class VABList;
    }

    namespace ecs
    {
        class TransformAssignmentBuffer;
    }
}

namespace hgl::ecs
{
    struct MaterialBatch;

    /// 本批 dispatch 档的**组大小**（唯一真源）：线（LineQuad）1 线程 1 段 ⇒ 64 段/组；
    /// 其余（VertexPassthrough）跨步协作 ⇒ 192 顶点/组。**GPU gather 侧不吃硬编码**：
    /// 该值经 `GPUCullSystem::SetBatchLayout` 下发进批信息行（`BatchInfoRow.group_size`）。
    inline uint32_t MeshDispatchGroupSize(const bool is_lines)
    {
        return is_lines ? graph::mtl::kMeshLineQuadMaxInvocations
                        : graph::mtl::kMeshVertexPassthroughMaxVertices;
    }


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

        bool material_is_mesh = false;                      ///<材质是否含 mesh stage（间接 flush 分派）
        const MaterialBatch *cur_owner_batch = nullptr;     ///<当前材质批（间接 flush 取 icb_mesh_tasks）
        uint32_t cur_pass_index = RenderPassSlot::Main;     ///<T4c3c：当前 render pass（切片按 pass 取）
        bool engaged_logged_ = false;                       ///<批 1.3 C5：一次性「间接 flush 已启用」日志闩（每实例，非进程级）

        /**
         * 处理间接渲染（mesh：一条 vkCmdDrawMeshTasksIndirectEXT multi-draw）
         */
        void ProcIndirectRender();


    public:
        PipelineMaterialRenderer(graph::ShaderProgram* m, graph::Pipeline* p);

        /**
         * 执行渲染
         * @param rcb 渲染命令缓冲
         * @param transform_buffer ECS Transform分配缓冲（可为空）
         * @param owner_batch 所属材质批（间接命令 + 参数表）
         */
        /// T4c4-3：CPU 绘制批（`DrawBatch`）已删 ⇒ 不再有 `batches`/`batch_count` 入参；
        /// 批的绘制一律走 GPU 剔除输出的切片（接管批），无 CPU 兜底。
        void Render(graph::RenderCmdBuffer* rcb,
                    TransformAssignmentBuffer* transform_buffer,
                    const MaterialBatch *owner_batch,
                    graph::RenderContext *render_context,
                    uint32_t camera_row,
                    uint32_t pass_index);   ///<T4c3c：本帧本 pass（切片/计数按 pass 取）
    };
}//namespace hgl::ecs
