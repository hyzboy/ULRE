#pragma once
#include<hgl/graph/VK.h>
#include<hgl/graph/VKVABList.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/type/ValueArray.h>

VK_NAMESPACE_BEGIN

class Material;
class Pipeline;
class RenderCmdBuffer;
class TransformAssignmentBuffer;
class MaterialInstanceAssignmentBuffer;
class GeometryDataBuffer;
class GeometryDrawRange;

/**
 * 绘制批次：将使用相同几何数据的节点合并为一个批次
 */
struct DrawBatch
{
            uint32_t                first_instance;     ///<第一个绘制实例(和instance渲染无关,对应InstanceRate的VAB)
            uint32_t                instance_count;     ///<此批次包含的实例数量

    const   GeometryDataBuffer *    geom_data_buffer;   ///<几何数据缓冲
    const   GeometryDrawRange *     geom_draw_range;    ///<绘制范围（顶点/索引偏移和数量）

    void Set(class Primitive *);
};//struct DrawBatch

using DrawBatchArray = ValueBuffer<DrawBatch>;

/**
 * Pipeline材质渲染器
 * 
 * 职责：
 * - 执行渲染命令
 * - 管理渲染状态（VAB绑定、IBO绑定等）
 * - 处理直接绘制和间接绘制
 */
class PipelineMaterialRenderer
{
private:
    // === 核心标识 ===
    Material *material;                         ///<材质
    Pipeline *pipeline;                         ///<管线

    // === 渲染状态缓存 ===
    RenderCmdBuffer *cmd_buf;                   ///<当前渲染命令缓冲
    VABList *vab_list;                          ///<顶点属性缓冲列表

    const GeometryDataBuffer *last_data_buffer; ///<上次绑定的几何数据缓冲
    const VDM *last_vdm;                        ///<上次使用的顶点数据管理器
    const GeometryDrawRange *last_draw_range;   ///<上次使用的绘制范围

    int first_indirect_draw_index;              ///<首个间接绘制索引
    uint indirect_draw_count;                   ///<累积的间接绘制数量

    // === 渲染辅助方法 ===
    bool BindVAB(const DrawBatch *, TransformAssignmentBuffer *, MaterialInstanceAssignmentBuffer *);    ///<绑定顶点属性缓冲
    void ProcIndirectRender(class IndirectDrawBuffer *, class IndirectDrawIndexedBuffer *);  ///<处理间接渲染
    bool Draw(DrawBatch *, TransformAssignmentBuffer *, MaterialInstanceAssignmentBuffer *, class IndirectDrawBuffer *, class IndirectDrawIndexedBuffer *);  ///<绘制单个批次

public:
    PipelineMaterialRenderer(Material *m, Pipeline *p);
    ~PipelineMaterialRenderer();

    /**
     * 执行渲染
     * @param rcb 渲染命令缓冲
     * @param batches 绘制批次数组
     * @param batch_count 批次数量
     * @param transform_buffer Transform分配缓冲（可为空）
     * @param mi_buffer 材质实例分配缓冲（可为空）
     * @param icb_draw 间接绘制缓冲（无索引）
     * @param icb_draw_indexed 间接绘制缓冲（有索引）
     */
    void Render(RenderCmdBuffer *rcb,
                const DrawBatchArray &batches,
                uint batch_count,
                TransformAssignmentBuffer *transform_buffer,
                MaterialInstanceAssignmentBuffer *mi_buffer,
                class IndirectDrawBuffer *icb_draw,
                class IndirectDrawIndexedBuffer *icb_draw_indexed);
};//class PipelineMaterialRenderer

VK_NAMESPACE_END
