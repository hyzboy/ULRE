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
}

PipelineMaterialRenderer::~PipelineMaterialRenderer()
{
    SAFE_CLEAR(vab_list);
}

bool PipelineMaterialRenderer::BindVAB(const DrawBatch *batch, TransformAssignmentBuffer *transform_buffer, MaterialInstanceAssignmentBuffer *mi_buffer)
{
    std::cerr << "[PipelineMaterialRenderer::BindVAB] === ENTRY ===" << std::endl;
    std::cerr << "[PipelineMaterialRenderer::BindVAB] Batch: " << batch << std::endl;
    std::cerr << "[PipelineMaterialRenderer::BindVAB] GeomDataBuffer: " << batch->geom_data_buffer << std::endl;
    
    // Log GeometryDataBuffer details
    if(batch->geom_data_buffer)
    {
        std::cerr << "[PipelineMaterialRenderer::BindVAB] GeomDataBuffer.vab_count: " << batch->geom_data_buffer->vab_count << std::endl;
        std::cerr << "[PipelineMaterialRenderer::BindVAB] GeomDataBuffer.vab_list: " << batch->geom_data_buffer->vab_list << std::endl;
        std::cerr << "[PipelineMaterialRenderer::BindVAB] GeomDataBuffer.vab_offset: " << batch->geom_data_buffer->vab_offset << std::endl;
        
        // Log each VAB
        for(uint32_t i = 0; i < batch->geom_data_buffer->vab_count; i++)
        {
            std::cerr << "[PipelineMaterialRenderer::BindVAB]   VAB[" << i << "]: buffer=" 
                      << batch->geom_data_buffer->vab_list[i] 
                      << ", offset=" << batch->geom_data_buffer->vab_offset[i] << std::endl;
        }
    }
    
    vab_list->Restart();
    std::cerr << "[PipelineMaterialRenderer::BindVAB] VABList restarted" << std::endl;

    if (!vab_list->Add(batch->geom_data_buffer))
    {
        std::cerr << "[PipelineMaterialRenderer::BindVAB] ERROR: Failed to add geometry data buffer to VABList!" << std::endl;
        return false;
    }
    std::cerr << "[PipelineMaterialRenderer::BindVAB] Geometry data buffer added to VABList" << std::endl;

    // 如果有Transform分配缓冲，绑定Transform索引VAB
    if (transform_buffer)
    {
        std::cerr << "[PipelineMaterialRenderer::BindVAB] Adding transform buffer..." << std::endl;
        VkBuffer transform_vab = transform_buffer->GetTransformVAB();
        if (!vab_list->Add(transform_vab, 0))
        {
            std::cerr << "[PipelineMaterialRenderer::BindVAB] ERROR: Failed to add transform VAB!" << std::endl;
            return false;
        }
        std::cerr << "[PipelineMaterialRenderer::BindVAB] Transform VAB added" << std::endl;
    }

    // 如果有MaterialInstance分配缓冲，绑定MaterialInstance索引VAB
    if (mi_buffer)
    {
        std::cerr << "[PipelineMaterialRenderer::BindVAB] Adding material instance buffer..." << std::endl;
        VkBuffer mi_vab = mi_buffer->GetMaterialInstanceVAB();
        if (!vab_list->Add(mi_vab, 0))
        {
            std::cerr << "[PipelineMaterialRenderer::BindVAB] ERROR: Failed to add MI VAB!" << std::endl;
            return false;
        }
        std::cerr << "[PipelineMaterialRenderer::BindVAB] Material instance VAB added" << std::endl;
    }

    std::cerr << "[PipelineMaterialRenderer::BindVAB] VABList IsFull: " << vab_list->IsFull() << std::endl;

    std::cerr << "[PipelineMaterialRenderer::BindVAB] Binding VABList to command buffer..." << std::endl;
    cmd_buf->BindVAB(vab_list);
    std::cerr << "[PipelineMaterialRenderer::BindVAB] VABList bound successfully" << std::endl;
    std::cerr << "[PipelineMaterialRenderer::BindVAB] === EXIT (success) ===" << std::endl;

    return true;
}

void PipelineMaterialRenderer::ProcIndirectRender(IndirectDrawBuffer *icb_draw, 
                                                   IndirectDrawIndexedBuffer *icb_draw_indexed)
{
    std::cerr << "[PipelineMaterialRenderer::ProcIndirectRender] === ENTRY ===" << std::endl;
    std::cerr << "[PipelineMaterialRenderer::ProcIndirectRender] First index: " << first_indirect_draw_index << std::endl;
    std::cerr << "[PipelineMaterialRenderer::ProcIndirectRender] Draw count: " << indirect_draw_count << std::endl;
    std::cerr << "[PipelineMaterialRenderer::ProcIndirectRender] Last data buffer: " << last_data_buffer << std::endl;
    
    // 提交累积的间接绘制命令
    // Note: last_data_buffer is guaranteed to be valid when this is called
    // because indirect_draw_count > 0 means we've processed at least one batch
    if (last_data_buffer->ibo)
    {
        std::cerr << "[PipelineMaterialRenderer::ProcIndirectRender] Using INDEXED indirect draw" << std::endl;
        std::cerr << "[PipelineMaterialRenderer::ProcIndirectRender] IBO: " << last_data_buffer->ibo << std::endl;
        icb_draw_indexed->DrawIndexed(*cmd_buf, first_indirect_draw_index, indirect_draw_count);
    }
    else
    {
        std::cerr << "[PipelineMaterialRenderer::ProcIndirectRender] Using NON-INDEXED indirect draw" << std::endl;
        icb_draw->Draw(*cmd_buf, first_indirect_draw_index, indirect_draw_count);
    }

    // 重置间接绘制状态
    first_indirect_draw_index = -1;
    indirect_draw_count = 0;
    
    std::cerr << "[PipelineMaterialRenderer::ProcIndirectRender] === EXIT ===" << std::endl;
}

bool PipelineMaterialRenderer::Draw(DrawBatch *batch,
                                     TransformAssignmentBuffer *transform_buffer,
                                     MaterialInstanceAssignmentBuffer *mi_buffer,
                                     IndirectDrawBuffer *icb_draw,
                                     IndirectDrawIndexedBuffer *icb_draw_indexed)
{
    std::cerr << "[PipelineMaterialRenderer::Draw] === ENTRY ===" << std::endl;
    std::cerr << "[PipelineMaterialRenderer::Draw] Batch: " << batch << std::endl;
    std::cerr << "[PipelineMaterialRenderer::Draw]   first_instance: " << batch->first_instance << std::endl;
    std::cerr << "[PipelineMaterialRenderer::Draw]   instance_count: " << batch->instance_count << std::endl;
    std::cerr << "[PipelineMaterialRenderer::Draw]   geom_data_buffer: " << batch->geom_data_buffer << std::endl;
    std::cerr << "[PipelineMaterialRenderer::Draw]   geom_draw_range: " << batch->geom_draw_range << std::endl;
    
    if (batch->geom_data_buffer)
    {
        std::cerr << "[PipelineMaterialRenderer::Draw]   DataBuffer.vdm: " << batch->geom_data_buffer->vdm << std::endl;
        std::cerr << "[PipelineMaterialRenderer::Draw]   DataBuffer.ibo: " << batch->geom_data_buffer->ibo << std::endl;
    }
    
    // 检查是否需要切换几何数据缓冲
    const bool need_buffer_switch = !last_data_buffer || 
                                   *(batch->geom_data_buffer) != *last_data_buffer;

    std::cerr << "[PipelineMaterialRenderer::Draw] need_buffer_switch: " << need_buffer_switch << std::endl;
    std::cerr << "[PipelineMaterialRenderer::Draw] last_data_buffer: " << last_data_buffer << std::endl;

    if (need_buffer_switch)
    {
        std::cerr << "[PipelineMaterialRenderer::Draw] Buffer switch required!" << std::endl;
        
        // 先提交之前累积的间接绘制
        if (indirect_draw_count)
        {
            std::cerr << "[PipelineMaterialRenderer::Draw] Flushing " << indirect_draw_count << " pending indirect draws..." << std::endl;
            ProcIndirectRender(icb_draw, icb_draw_indexed);
        }

        // 更新缓冲状态
        last_data_buffer = batch->geom_data_buffer;
        last_draw_range = nullptr;

        std::cerr << "[PipelineMaterialRenderer::Draw] Binding VAB..." << std::endl;
        // 绑定新的顶点数组缓冲
        if (!BindVAB(batch, transform_buffer, mi_buffer))
        {
            std::cerr << "[PipelineMaterialRenderer::Draw] ERROR: BindVAB failed!" << std::endl;
            return false;
        }
        std::cerr << "[PipelineMaterialRenderer::Draw] VAB bound successfully" << std::endl;

        // 如果有索引缓冲，也需要绑定
        if (batch->geom_data_buffer->ibo)
        {
            std::cerr << "[PipelineMaterialRenderer::Draw] Binding IBO: " << batch->geom_data_buffer->ibo << std::endl;
            cmd_buf->BindIBO(batch->geom_data_buffer->ibo);
            std::cerr << "[PipelineMaterialRenderer::Draw] IBO bound" << std::endl;
        }
        else
        {
            std::cerr << "[PipelineMaterialRenderer::Draw] No IBO to bind" << std::endl;
        }
    }
    else
    {
        std::cerr << "[PipelineMaterialRenderer::Draw] Using cached buffer (no switch)" << std::endl;
    }

    // 提交绘制命令
    if (batch->geom_data_buffer->vdm)
    {
        std::cerr << "[PipelineMaterialRenderer::Draw] Using INDIRECT draw (has VDM)" << std::endl;
        // 间接绘制：累积命令
        if (indirect_draw_count == 0)
        {
            first_indirect_draw_index = batch->first_instance;
            std::cerr << "[PipelineMaterialRenderer::Draw] Starting new indirect batch at index " << first_indirect_draw_index << std::endl;
        }

        ++indirect_draw_count;
        std::cerr << "[PipelineMaterialRenderer::Draw] Accumulated indirect draw count: " << indirect_draw_count << std::endl;
    }
    else
    {
        std::cerr << "[PipelineMaterialRenderer::Draw] Using DIRECT draw (no VDM)" << std::endl;
        std::cerr << "[PipelineMaterialRenderer::Draw] Draw parameters:" << std::endl;
        std::cerr << "[PipelineMaterialRenderer::Draw]   instance_count: " << batch->instance_count << std::endl;
        std::cerr << "[PipelineMaterialRenderer::Draw]   first_instance: " << batch->first_instance << std::endl;
        
        // 直接绘制：立即提交
        cmd_buf->Draw(batch->geom_data_buffer, batch->geom_draw_range, 
                     batch->instance_count, batch->first_instance);
        std::cerr << "[PipelineMaterialRenderer::Draw] Direct draw submitted" << std::endl;
    }

    std::cerr << "[PipelineMaterialRenderer::Draw] === EXIT (success) ===" << std::endl;
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
    std::cerr << "[PipelineMaterialRenderer::Render] === ENTRY ===" << std::endl;
    std::cerr << "[PipelineMaterialRenderer::Render] RenderCmdBuffer: " << rcb << std::endl;
    std::cerr << "[PipelineMaterialRenderer::Render] Batch count: " << batch_count << std::endl;
    std::cerr << "[PipelineMaterialRenderer::Render] Material: " << material << std::endl;
    std::cerr << "[PipelineMaterialRenderer::Render] Pipeline: " << pipeline << std::endl;
    std::cerr << "[PipelineMaterialRenderer::Render] Transform buffer: " << transform_buffer << std::endl;
    std::cerr << "[PipelineMaterialRenderer::Render] MI buffer: " << mi_buffer << std::endl;
    std::cerr << "[PipelineMaterialRenderer::Render] ICB Draw: " << icb_draw << std::endl;
    std::cerr << "[PipelineMaterialRenderer::Render] ICB Draw Indexed: " << icb_draw_indexed << std::endl;
    
    // 前置条件检查
    if (!rcb)
    {
        std::cerr << "[PipelineMaterialRenderer::Render] ERROR: No render command buffer!" << std::endl;
        return;
    }
    if (batch_count <= 0)
    {
        std::cerr << "[PipelineMaterialRenderer::Render] WARNING: No batches to render!" << std::endl;
        return;
    }

    cmd_buf = rcb;

    // 绑定管线
    std::cerr << "[PipelineMaterialRenderer::Render] Binding pipeline..." << std::endl;
    cmd_buf->BindPipeline(pipeline);
    std::cerr << "[PipelineMaterialRenderer::Render] Pipeline bound" << std::endl;

    // 重置渲染状态缓存
    last_data_buffer = nullptr;
    last_vdm = nullptr;
    last_draw_range = nullptr;
    indirect_draw_count = 0;
    first_indirect_draw_index = -1;
    std::cerr << "[PipelineMaterialRenderer::Render] Render state cache reset" << std::endl;

    // 绑定Transform分配缓冲（如果有）
    if (transform_buffer)
    {
        std::cerr << "[PipelineMaterialRenderer::Render] Binding transform buffer..." << std::endl;
        transform_buffer->BindTransform(material);
        std::cerr << "[PipelineMaterialRenderer::Render] Transform buffer bound" << std::endl;
    }
    
    // 绑定MaterialInstance分配缓冲（如果有）
    if (mi_buffer)
    {
        std::cerr << "[PipelineMaterialRenderer::Render] Binding material instance buffer..." << std::endl;
        mi_buffer->BindMaterialInstance(material);
        std::cerr << "[PipelineMaterialRenderer::Render] Material instance buffer bound" << std::endl;
    }

    // 绑定材质描述符集
    std::cerr << "[PipelineMaterialRenderer::Render] Binding material descriptor sets..." << std::endl;
    cmd_buf->BindDescriptorSets(material);
    std::cerr << "[PipelineMaterialRenderer::Render] Descriptor sets bound" << std::endl;

    // 遍历绘制批次 - batches is const, but Draw modifies internal state only
    DrawBatch *batch = const_cast<DrawBatch *>(batches.GetData());
    std::cerr << "[PipelineMaterialRenderer::Render] Processing " << batch_count << " batches..." << std::endl;
    
    for (uint i = 0; i < batch_count; i++)
    {
        std::cerr << "[PipelineMaterialRenderer::Render] === Processing batch " << i << " ===" << std::endl;
        Draw(batch, transform_buffer, mi_buffer, icb_draw, icb_draw_indexed);
        ++batch;
    }
    
    // 提交剩余的间接绘制命令
    if (indirect_draw_count)
    {
        std::cerr << "[PipelineMaterialRenderer::Render] Flushing remaining " << indirect_draw_count << " indirect draws..." << std::endl;
        ProcIndirectRender(icb_draw, icb_draw_indexed);
    }
    
    std::cerr << "[PipelineMaterialRenderer::Render] === EXIT ===" << std::endl;
}

VK_NAMESPACE_END
