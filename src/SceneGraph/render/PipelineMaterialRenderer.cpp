#include<hgl/graph/PipelineMaterialRenderer.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKVertexInput.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKIndirectCommandBuffer.h>
#include"InstanceAssignmentBuffer.h"

VK_NAMESPACE_BEGIN

void DrawBatch::Set(Primitive *primitive)
{
    geom_data_buffer = primitive->GetDataBuffer();
    geom_draw_range = primitive->GetRenderData();
}

PipelineMaterialRenderer::PipelineMaterialRenderer(Material *m, Pipeline *p)
    : material(m)
    , pipeline(p)
    , cmd_buf(nullptr)
    , vab_list(new VABList(m->GetVertexInput()->GetCount()))
    , last_data_buffer(nullptr)
    , last_vdm(nullptr)
    , last_draw_range(nullptr)
    , first_indirect_draw_index(-1)
    , indirect_draw_count(0)
{
}

PipelineMaterialRenderer::~PipelineMaterialRenderer()
{
    SAFE_CLEAR(vab_list);
}

bool PipelineMaterialRenderer::BindVAB(const DrawBatch *batch, InstanceAssignmentBuffer *assign_buffer)
{
    vab_list->Restart();

    if (!vab_list->Add(batch->geom_data_buffer))
        return false;

    // 如果有实例分配缓冲（LocalToWorld/MI数据），也需要绑定
    if (assign_buffer)
    {
        if (!vab_list->Add(assign_buffer->GetVAB(), 0))
            return false;
    }

    cmd_buf->BindVAB(vab_list);

    return true;
}

void PipelineMaterialRenderer::ProcIndirectRender(IndirectDrawBuffer *icb_draw, 
                                                   IndirectDrawIndexedBuffer *icb_draw_indexed)
{
    // 提交累积的间接绘制命令
    // Note: last_data_buffer is guaranteed to be valid when this is called
    // because indirect_draw_count > 0 means we've processed at least one batch
    if (last_data_buffer->ibo)
        icb_draw_indexed->DrawIndexed(*cmd_buf, first_indirect_draw_index, indirect_draw_count);
    else
        icb_draw->Draw(*cmd_buf, first_indirect_draw_index, indirect_draw_count);

    // 重置间接绘制状态
    first_indirect_draw_index = -1;
    indirect_draw_count = 0;
}

bool PipelineMaterialRenderer::Draw(DrawBatch *batch,
                                     InstanceAssignmentBuffer *assign_buffer,
                                     IndirectDrawBuffer *icb_draw,
                                     IndirectDrawIndexedBuffer *icb_draw_indexed)
{
    // 检查是否需要切换几何数据缓冲
    const bool need_buffer_switch = !last_data_buffer || 
                                   *(batch->geom_data_buffer) != *last_data_buffer;

    if (need_buffer_switch)
    {
        // 先提交之前累积的间接绘制
        if (indirect_draw_count)
            ProcIndirectRender(icb_draw, icb_draw_indexed);

        // 更新缓冲状态
        last_data_buffer = batch->geom_data_buffer;
        last_draw_range = nullptr;

        // 绑定新的顶点数组缓冲
        if (!BindVAB(batch, assign_buffer))
            return false;

        // 如果有索引缓冲，也需要绑定
        if (batch->geom_data_buffer->ibo)
            cmd_buf->BindIBO(batch->geom_data_buffer->ibo);
    }

    // 提交绘制命令
    if (batch->geom_data_buffer->vdm)
    {
        // 间接绘制：累积命令
        if (indirect_draw_count == 0)
            first_indirect_draw_index = batch->first_instance;

        ++indirect_draw_count;
    }
    else
    {
        // 直接绘制：立即提交
        cmd_buf->Draw(batch->geom_data_buffer, batch->geom_draw_range, 
                     batch->instance_count, batch->first_instance);
    }

    return true;
}

void PipelineMaterialRenderer::Render(RenderCmdBuffer *rcb,
                                       const DrawBatchArray &batches,
                                       uint batch_count,
                                       InstanceAssignmentBuffer *assign_buffer,
                                       IndirectDrawBuffer *icb_draw,
                                       IndirectDrawIndexedBuffer *icb_draw_indexed)
{
    // 前置条件检查
    if (!rcb) return;
    if (batch_count <= 0) return;

    cmd_buf = rcb;

    // 绑定管线
    cmd_buf->BindPipeline(pipeline);

    // 重置渲染状态缓存
    last_data_buffer = nullptr;
    last_vdm = nullptr;
    last_draw_range = nullptr;

    // 绑定实例分配缓冲（如果有）
    if (assign_buffer)
        assign_buffer->Bind(material);

    // 绑定材质描述符集
    cmd_buf->BindDescriptorSets(material);

    // 遍历绘制批次 - batches is const, but Draw modifies internal state only
    DrawBatch *batch = const_cast<DrawBatch *>(batches.GetData());
    for (uint i = 0; i < batch_count; i++)
    {
        Draw(batch, assign_buffer, icb_draw, icb_draw_indexed);
        ++batch;
    }
    
    // 提交剩余的间接绘制命令
    if (indirect_draw_count)
        ProcIndirectRender(icb_draw, icb_draw_indexed);
}

VK_NAMESPACE_END
