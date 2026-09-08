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

    // mesh shader 组数计算（与 MeshTemplateEmitter 的 dispatch 约定一致）：
    // Lines（LineQuad）每线程 1 线段 = 2 顶点 → 线段数 = total/2，组大小 64；
    // 其它（VertexPassthrough）每线程 1 顶点，组大小 96（3 的倍数——组内
    // 三角形永不跨组，避免 64 边界丢三角形）
    inline uint32_t CalcMeshGroupCount(const bool is_lines, const uint32_t total_vertices)
    {
        const uint32_t process_count = is_lines ? (total_vertices >> 1u) : total_vertices;
        const uint32_t group_size = is_lines ? 64u : 96u;
        return (process_count + group_size - 1u) / group_size;
    }

    /**
     * 绘制批次：将使用相同几何数据的节点合并为一个批次
     */
    struct DrawBatch
    {
                uint32_t                first_instance = 0;     ///<第一个绘制实例(和instance渲染无关,对应InstanceRate的VAB)
                uint32_t                instance_count = 0;     ///<此批次包含的实例数量

        const   graph::GeometryDataBuffer *    geom_data_buffer = nullptr;   ///<几何数据缓冲
        const   graph::GeometryDrawRange *     geom_draw_range = nullptr;    ///<绘制范围（顶点/索引偏移和数量）
        const   graph::Geometry *              geometry = nullptr;           ///<几何体（BDA 地址直取——行内填 addr_*）

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

        bool material_is_mesh = false;                      ///<材质是否含 mesh stage（间接 flush 分派）
        const MaterialBatch *cur_owner_batch = nullptr;     ///<当前材质批（间接 flush 取 icb_mesh_tasks）

        int first_indirect_draw_index;                      ///<首个间接绘制索引
        uint32_t indirect_draw_count;                       ///<累积的间接绘制数量
        uint32_t indirect_draw_command_offset = 0;          ///<本批次已提交的间接命令数（ICB 命令序号累计）
        /**
         * 处理间接渲染（mesh：一条 vkCmdDrawMeshTasksIndirectEXT multi-draw）
         */
        void ProcIndirectRender();

        /**
         * 绘制单个批次
         * @param batch 绘制批次
         * @param transform_buffer ECS Transform分配缓冲
         * @param owner_batch 所属材质批（间接命令 + 参数表）
         * @return 绘制是否成功
         */
        bool Draw(DrawBatch* batch,
                  TransformAssignmentBuffer* transform_buffer,
                  const MaterialBatch *owner_batch = nullptr);

    public:
        PipelineMaterialRenderer(graph::ShaderProgram* m, graph::Pipeline* p);

        /**
         * 执行渲染
         * @param rcb 渲染命令缓冲
         * @param batches 绘制批次数组
         * @param batch_count 批次数量
         * @param transform_buffer ECS Transform分配缓冲（可为空）
         * @param owner_batch 所属材质批（间接命令 + 参数表）
         */
        void Render(graph::RenderCmdBuffer* rcb,
                    const DrawBatchArray& batches,
                    uint32_t batch_count,
                    TransformAssignmentBuffer* transform_buffer,
                    const MaterialBatch *owner_batch = nullptr,
                    graph::RenderContext *render_context = nullptr);
    };
}//namespace hgl::ecs
