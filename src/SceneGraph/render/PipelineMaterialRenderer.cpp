#include<hgl/graph/PipelineMaterialRenderer.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKVertexInput.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKIndirectCommandBuffer.h>
#include"TransformAssignmentBuffer.h"
#include"MaterialInstanceAssignmentBuffer.h"
#include<iostream>

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
    std::cout << "[PipelineMaterialRenderer] Constructor - Material: " << (void*)m 
              << ", Pipeline: " << (void*)p 
              << ", Expected VAB count: " << m->GetVertexInput()->GetCount() << std::endl;
}

PipelineMaterialRenderer::~PipelineMaterialRenderer()
{
    SAFE_CLEAR(vab_list);
}

bool PipelineMaterialRenderer::BindVAB(const DrawBatch *batch, TransformAssignmentBuffer *transform_buffer, MaterialInstanceAssignmentBuffer *mi_buffer)
{
    std::cout << "[PipelineMaterialRenderer::BindVAB] === Starting VAB binding ===" << std::endl;
    std::cout << "[PipelineMaterialRenderer::BindVAB] Batch: " << (void*)batch << std::endl;
    std::cout << "[PipelineMaterialRenderer::BindVAB] TransformBuffer: " << (void*)transform_buffer << std::endl;
    std::cout << "[PipelineMaterialRenderer::BindVAB] MaterialInstanceBuffer: " << (void*)mi_buffer << std::endl;
    
    vab_list->Restart();

    std::cout << "[PipelineMaterialRenderer::BindVAB] Adding geometry data buffer..." << std::endl;
    if (!vab_list->Add(batch->geom_data_buffer))
    {
        std::cout << "[PipelineMaterialRenderer::BindVAB] ERROR: Failed to add geometry data buffer! VAB list is full!" << std::endl;
        std::cout << "[PipelineMaterialRenderer::BindVAB] GeomDataBuffer VAB count: " << batch->geom_data_buffer->vab_count << std::endl;
        return false;
    }
    std::cout << "[PipelineMaterialRenderer::BindVAB] Geometry data buffer added successfully" << std::endl;

    // 如果有Transform分配缓冲，绑定Transform索引VAB
    if (transform_buffer)
    {
        VkBuffer transform_vab = transform_buffer->GetTransformVAB();
        std::cout << "[PipelineMaterialRenderer::BindVAB] Adding Transform VAB: " << (void*)transform_vab << std::endl;
        if (!vab_list->Add(transform_vab, 0))
        {
            std::cout << "[PipelineMaterialRenderer::BindVAB] ERROR: Failed to add Transform VAB! VAB list is full!" << std::endl;
            return false;
        }
        std::cout << "[PipelineMaterialRenderer::BindVAB] Transform VAB added successfully" << std::endl;
    }
    else
    {
        std::cout << "[PipelineMaterialRenderer::BindVAB] No Transform buffer provided" << std::endl;
    }

    // 如果有MaterialInstance分配缓冲，绑定MaterialInstance索引VAB
    if (mi_buffer)
    {
        VkBuffer mi_vab = mi_buffer->GetMaterialInstanceVAB();
        std::cout << "[PipelineMaterialRenderer::BindVAB] Adding MaterialInstance VAB: " << (void*)mi_vab << std::endl;
        if (!vab_list->Add(mi_vab, 0))
        {
            std::cout << "[PipelineMaterialRenderer::BindVAB] ERROR: Failed to add MaterialInstance VAB! VAB list is full!" << std::endl;
            return false;
        }
        std::cout << "[PipelineMaterialRenderer::BindVAB] MaterialInstance VAB added successfully" << std::endl;
    }
    else
    {
        std::cout << "[PipelineMaterialRenderer::BindVAB] No MaterialInstance buffer provided" << std::endl;
    }

    std::cout << "[PipelineMaterialRenderer::BindVAB] Binding VAB list to command buffer..." << std::endl;
    cmd_buf->BindVAB(vab_list);
    std::cout << "[PipelineMaterialRenderer::BindVAB] === VAB binding complete ===" << std::endl;

    return true;
}

void PipelineMaterialRenderer::ProcIndirectRender(IndirectDrawBuffer *icb_draw, 
                                                   IndirectDrawIndexedBuffer *icb_draw_indexed)
{
    std::cout << "[PipelineMaterialRenderer::ProcIndirectRender] Processing indirect render..." << std::endl;
    std::cout << "[PipelineMaterialRenderer::ProcIndirectRender] first_indirect_draw_index: " << first_indirect_draw_index << std::endl;
    std::cout << "[PipelineMaterialRenderer::ProcIndirectRender] indirect_draw_count: " << indirect_draw_count << std::endl;
    std::cout << "[PipelineMaterialRenderer::ProcIndirectRender] last_data_buffer has IBO: " << (last_data_buffer && last_data_buffer->ibo ? "YES" : "NO") << std::endl;
    
    // 提交累积的间接绘制命令
    // Note: last_data_buffer is guaranteed to be valid when this is called
    // because indirect_draw_count > 0 means we've processed at least one batch
    if (last_data_buffer->ibo)
    {
        std::cout << "[PipelineMaterialRenderer::ProcIndirectRender] Calling DrawIndexed..." << std::endl;
        icb_draw_indexed->DrawIndexed(*cmd_buf, first_indirect_draw_index, indirect_draw_count);
    }
    else
    {
        std::cout << "[PipelineMaterialRenderer::ProcIndirectRender] Calling Draw..." << std::endl;
        icb_draw->Draw(*cmd_buf, first_indirect_draw_index, indirect_draw_count);
    }

    // 重置间接绘制状态
    first_indirect_draw_index = -1;
    indirect_draw_count = 0;
    std::cout << "[PipelineMaterialRenderer::ProcIndirectRender] Indirect render complete, state reset" << std::endl;
}

bool PipelineMaterialRenderer::Draw(DrawBatch *batch,
                                     TransformAssignmentBuffer *transform_buffer,
                                     MaterialInstanceAssignmentBuffer *mi_buffer,
                                     IndirectDrawBuffer *icb_draw,
                                     IndirectDrawIndexedBuffer *icb_draw_indexed)
{
    std::cout << "[PipelineMaterialRenderer::Draw] Processing batch..." << std::endl;
    std::cout << "[PipelineMaterialRenderer::Draw] Batch geom_data_buffer: " << (void*)batch->geom_data_buffer << std::endl;
    std::cout << "[PipelineMaterialRenderer::Draw] Batch first_instance: " << batch->first_instance << ", instance_count: " << batch->instance_count << std::endl;
    
    // 检查是否需要切换几何数据缓冲
    const bool need_buffer_switch = !last_data_buffer || 
                                   *(batch->geom_data_buffer) != *last_data_buffer;

    std::cout << "[PipelineMaterialRenderer::Draw] Need buffer switch: " << (need_buffer_switch ? "YES" : "NO") << std::endl;

    if (need_buffer_switch)
    {
        // 先提交之前累积的间接绘制
        if (indirect_draw_count)
        {
            std::cout << "[PipelineMaterialRenderer::Draw] Submitting accumulated indirect draws (" << indirect_draw_count << " commands)" << std::endl;
            ProcIndirectRender(icb_draw, icb_draw_indexed);
        }

        // 更新缓冲状态
        last_data_buffer = batch->geom_data_buffer;
        last_draw_range = nullptr;

        std::cout << "[PipelineMaterialRenderer::Draw] Binding VABs for new buffer..." << std::endl;
        // 绑定新的顶点数组缓冲
        if (!BindVAB(batch, transform_buffer, mi_buffer))
        {
            std::cout << "[PipelineMaterialRenderer::Draw] ERROR: BindVAB failed!" << std::endl;
            return false;
        }

        // 如果有索引缓冲，也需要绑定
        if (batch->geom_data_buffer->ibo)
        {
            std::cout << "[PipelineMaterialRenderer::Draw] Binding IBO: " << (void*)batch->geom_data_buffer->ibo << std::endl;
            cmd_buf->BindIBO(batch->geom_data_buffer->ibo);
        }
        else
        {
            std::cout << "[PipelineMaterialRenderer::Draw] No IBO to bind" << std::endl;
        }
    }

    // 提交绘制命令
    if (batch->geom_data_buffer->vdm)
    {
        std::cout << "[PipelineMaterialRenderer::Draw] Using INDIRECT rendering (VDM present)" << std::endl;
        // 间接绘制：累积命令
        if (indirect_draw_count == 0)
        {
            first_indirect_draw_index = batch->first_instance;
            std::cout << "[PipelineMaterialRenderer::Draw] Starting new indirect batch at index " << first_indirect_draw_index << std::endl;
        }

        ++indirect_draw_count;
        std::cout << "[PipelineMaterialRenderer::Draw] Accumulated indirect draw count: " << indirect_draw_count << std::endl;
    }
    else
    {
        std::cout << "[PipelineMaterialRenderer::Draw] Using DIRECT rendering (NO VDM)" << std::endl;
        std::cout << "[PipelineMaterialRenderer::Draw] Drawing directly - geom_data_buffer: " << (void*)batch->geom_data_buffer 
                  << ", draw_range: " << (void*)batch->geom_draw_range
                  << ", instance_count: " << batch->instance_count
                  << ", first_instance: " << batch->first_instance << std::endl;
        // 直接绘制：立即提交
        cmd_buf->Draw(batch->geom_data_buffer, batch->geom_draw_range, 
                     batch->instance_count, batch->first_instance);
        std::cout << "[PipelineMaterialRenderer::Draw] Direct draw command submitted" << std::endl;
    }

    return true;
}

void PipelineMaterialRenderer::Render(RenderCmdBuffer *rcb,
                                       const DrawBatchArray &batches,
                                       uint batch_count,
                                       TransformAssignmentBuffer *transform_buffer,
                                       MaterialInstanceAssignmentBuffer *mi_buffer,
                                       IndirectDrawBuffer *icb_draw,
                                       IndirectDrawIndexedBuffer *icb_draw_indexed)
{
    std::cout << "[PipelineMaterialRenderer::Render] === Starting render ===" << std::endl;
    std::cout << "[PipelineMaterialRenderer::Render] CmdBuffer: " << (void*)rcb << std::endl;
    std::cout << "[PipelineMaterialRenderer::Render] Batch count: " << batch_count << std::endl;
    std::cout << "[PipelineMaterialRenderer::Render] TransformBuffer: " << (void*)transform_buffer << std::endl;
    std::cout << "[PipelineMaterialRenderer::Render] MaterialInstanceBuffer: " << (void*)mi_buffer << std::endl;
    std::cout << "[PipelineMaterialRenderer::Render] IndirectDrawBuffer: " << (void*)icb_draw << std::endl;
    std::cout << "[PipelineMaterialRenderer::Render] IndirectDrawIndexedBuffer: " << (void*)icb_draw_indexed << std::endl;
    
    // 前置条件检查
    if (!rcb)
    {
        std::cout << "[PipelineMaterialRenderer::Render] ERROR: No command buffer!" << std::endl;
        return;
    }
    if (batch_count <= 0)
    {
        std::cout << "[PipelineMaterialRenderer::Render] No batches to render" << std::endl;
        return;
    }

    cmd_buf = rcb;

    std::cout << "[PipelineMaterialRenderer::Render] Binding pipeline: " << (void*)pipeline << std::endl;
    // 绑定管线
    cmd_buf->BindPipeline(pipeline);

    // 重置渲染状态缓存
    last_data_buffer = nullptr;
    last_vdm = nullptr;
    last_draw_range = nullptr;

    std::cout << "[PipelineMaterialRenderer::Render] Binding Transform/MaterialInstance buffers to material..." << std::endl;
    // 绑定Transform分配缓冲（如果有）
    if (transform_buffer)
    {
        std::cout << "[PipelineMaterialRenderer::Render] Binding Transform buffer to material" << std::endl;
        transform_buffer->BindTransform(material);
    }
    else
    {
        std::cout << "[PipelineMaterialRenderer::Render] No Transform buffer to bind" << std::endl;
    }
    
    // 绑定MaterialInstance分配缓冲（如果有）
    if (mi_buffer)
    {
        std::cout << "[PipelineMaterialRenderer::Render] Binding MaterialInstance buffer to material" << std::endl;
        mi_buffer->BindMaterialInstance(material);
    }
    else
    {
        std::cout << "[PipelineMaterialRenderer::Render] No MaterialInstance buffer to bind" << std::endl;
    }

    std::cout << "[PipelineMaterialRenderer::Render] Binding material descriptor sets..." << std::endl;
    // 绑定材质描述符集
    cmd_buf->BindDescriptorSets(material);

    std::cout << "[PipelineMaterialRenderer::Render] Processing " << batch_count << " batches..." << std::endl;
    // 遍历绘制批次 - batches is const, but Draw modifies internal state only
    DrawBatch *batch = const_cast<DrawBatch *>(batches.GetData());
    for (uint i = 0; i < batch_count; i++)
    {
        std::cout << "[PipelineMaterialRenderer::Render] ========== Batch " << i << " of " << batch_count << " ==========" << std::endl;
        Draw(batch, transform_buffer, mi_buffer, icb_draw, icb_draw_indexed);
        ++batch;
    }
    
    // 提交剩余的间接绘制命令
    if (indirect_draw_count)
    {
        std::cout << "[PipelineMaterialRenderer::Render] Submitting remaining indirect draws (" << indirect_draw_count << " commands)" << std::endl;
        ProcIndirectRender(icb_draw, icb_draw_indexed);
    }
    
    std::cout << "[PipelineMaterialRenderer::Render] === Render complete ===" << std::endl;
}

VK_NAMESPACE_END
