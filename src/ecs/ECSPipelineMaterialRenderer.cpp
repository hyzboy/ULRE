/**
 * ECSPipelineMaterialRenderer.cpp - ECS Pipeline材质渲染器实现
 * 
 * 参照 PipelineMaterialRenderer 实现，但使用 ECS 版本的 Assignment Buffers
 */

#include"ECSPipelineMaterialRenderer.h"
#include"ECSTransformAssignmentBuffer.h"
#include"ECSMaterialInstanceAssignmentBuffer.h"
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKVertexInput.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKIndirectCommandBuffer.h>
#include<iostream>

namespace hgl::ecs
{
    ECSPipelineMaterialRenderer::ECSPipelineMaterialRenderer(graph::Material* m, graph::Pipeline* p)
        : material(m)
        , pipeline(p)
        , cmd_buf(nullptr)
        , vab_list(new graph::VABList(m->GetVertexInput()->GetCount()))
        , last_data_buffer(nullptr)
        , last_vdm(nullptr)
        , last_draw_range(nullptr)
        , first_indirect_draw_index(-1)
        , indirect_draw_count(0)
    {
        std::cout << "[ECSPipelineMaterialRenderer] Constructor - Material: " << (void*)material 
                  << ", Pipeline: " << (void*)pipeline << std::endl;
    }

    ECSPipelineMaterialRenderer::~ECSPipelineMaterialRenderer()
    {
        std::cout << "[ECSPipelineMaterialRenderer] Destructor" << std::endl;
        SAFE_CLEAR(vab_list);
    }

    bool ECSPipelineMaterialRenderer::BindVAB(const graph::DrawBatch* batch, 
                                               ECSTransformAssignmentBuffer* transform_buffer, 
                                               ECSMaterialInstanceAssignmentBuffer* mi_buffer)
    {
        std::cout << "[ECSPipelineMaterialRenderer::BindVAB] === ENTRY ===" << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::BindVAB] Batch: " << (void*)batch << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::BindVAB] GeomDataBuffer: " << (void*)batch->geom_data_buffer << std::endl;
        
        // Log GeometryDataBuffer details
        if (batch->geom_data_buffer)
        {
            std::cout << "[ECSPipelineMaterialRenderer::BindVAB] GeomDataBuffer.vab_count: " << batch->geom_data_buffer->vab_count << std::endl;
            std::cout << "[ECSPipelineMaterialRenderer::BindVAB] GeomDataBuffer.vab_list: " << (void*)batch->geom_data_buffer->vab_list << std::endl;
            std::cout << "[ECSPipelineMaterialRenderer::BindVAB] GeomDataBuffer.vab_offset: " << (void*)batch->geom_data_buffer->vab_offset << std::endl;
            
            // Log each VAB
            for (uint32_t i = 0; i < batch->geom_data_buffer->vab_count; i++)
            {
                std::cout << "[ECSPipelineMaterialRenderer::BindVAB]   VAB[" << i << "]: buffer=" 
                          << batch->geom_data_buffer->vab_list[i] 
                          << ", offset=" << batch->geom_data_buffer->vab_offset[i] << std::endl;
            }
        }
        
        vab_list->Restart();
        std::cout << "[ECSPipelineMaterialRenderer::BindVAB] VABList restarted" << std::endl;

        // 添加几何数据的VAB
        if (!vab_list->Add(batch->geom_data_buffer))
        {
            std::cout << "[ECSPipelineMaterialRenderer::BindVAB] ERROR: Failed to add geometry data buffer to VABList!" << std::endl;
            return false;
        }
        std::cout << "[ECSPipelineMaterialRenderer::BindVAB] Geometry data buffer added to VABList" << std::endl;

        // 如果有ECS Transform分配缓冲，绑定Transform索引VAB
        if (transform_buffer)
        {
            std::cout << "[ECSPipelineMaterialRenderer::BindVAB] Adding ECS transform buffer..." << std::endl;
            VkBuffer transform_vab = transform_buffer->GetTransformVAB();
            
            if (transform_vab == VK_NULL_HANDLE)
            {
                std::cout << "[ECSPipelineMaterialRenderer::BindVAB] WARNING: Transform VAB is null!" << std::endl;
            }
            else
            {
                if (!vab_list->Add(transform_vab, 0))
                {
                    std::cout << "[ECSPipelineMaterialRenderer::BindVAB] ERROR: Failed to add ECS transform VAB!" << std::endl;
                    return false;
                }
                std::cout << "[ECSPipelineMaterialRenderer::BindVAB] ECS Transform VAB added" << std::endl;
            }
        }

        // 如果有ECS MaterialInstance分配缓冲，绑定MaterialInstance索引VAB
        if (mi_buffer)
        {
            std::cout << "[ECSPipelineMaterialRenderer::BindVAB] Adding ECS material instance buffer..." << std::endl;
            VkBuffer mi_vab = mi_buffer->GetMaterialInstanceVAB();
            
            if (mi_vab == VK_NULL_HANDLE)
            {
                std::cout << "[ECSPipelineMaterialRenderer::BindVAB] WARNING: MI VAB is null!" << std::endl;
            }
            else
            {
                if (!vab_list->Add(mi_vab, 0))
                {
                    std::cout << "[ECSPipelineMaterialRenderer::BindVAB] ERROR: Failed to add ECS MI VAB!" << std::endl;
                    return false;
                }
                std::cout << "[ECSPipelineMaterialRenderer::BindVAB] ECS Material instance VAB added" << std::endl;
            }
        }

        std::cout << "[ECSPipelineMaterialRenderer::BindVAB] VABList IsFull: " << vab_list->IsFull() << std::endl;

        std::cout << "[ECSPipelineMaterialRenderer::BindVAB] Binding VABList to command buffer..." << std::endl;
        cmd_buf->BindVAB(vab_list);
        std::cout << "[ECSPipelineMaterialRenderer::BindVAB] VABList bound successfully" << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::BindVAB] === EXIT (success) ===" << std::endl;

        return true;
    }

    void ECSPipelineMaterialRenderer::ProcIndirectRender(graph::IndirectDrawBuffer* icb_draw, 
                                                         graph::IndirectDrawIndexedBuffer* icb_draw_indexed)
    {
        std::cout << "[ECSPipelineMaterialRenderer::ProcIndirectRender] === ENTRY ===" << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::ProcIndirectRender] First index: " << first_indirect_draw_index << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::ProcIndirectRender] Draw count: " << indirect_draw_count << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::ProcIndirectRender] Last data buffer: " << (void*)last_data_buffer << std::endl;
        
        // 提交累积的间接绘制命令
        if (last_data_buffer->ibo)
        {
            std::cout << "[ECSPipelineMaterialRenderer::ProcIndirectRender] Using INDEXED indirect draw" << std::endl;
            std::cout << "[ECSPipelineMaterialRenderer::ProcIndirectRender] IBO: " << last_data_buffer->ibo << std::endl;
            icb_draw_indexed->DrawIndexed(*cmd_buf, first_indirect_draw_index, indirect_draw_count);
        }
        else
        {
            std::cout << "[ECSPipelineMaterialRenderer::ProcIndirectRender] Using NON-INDEXED indirect draw" << std::endl;
            icb_draw->Draw(*cmd_buf, first_indirect_draw_index, indirect_draw_count);
        }

        // 重置间接绘制状态
        first_indirect_draw_index = -1;
        indirect_draw_count = 0;
        
        std::cout << "[ECSPipelineMaterialRenderer::ProcIndirectRender] === EXIT ===" << std::endl;
    }

    bool ECSPipelineMaterialRenderer::Draw(graph::DrawBatch* batch,
                                            ECSTransformAssignmentBuffer* transform_buffer,
                                            ECSMaterialInstanceAssignmentBuffer* mi_buffer,
                                            graph::IndirectDrawBuffer* icb_draw,
                                            graph::IndirectDrawIndexedBuffer* icb_draw_indexed)
    {
        std::cout << "[ECSPipelineMaterialRenderer::Draw] === ENTRY ===" << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::Draw] Batch: " << (void*)batch << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::Draw]   first_instance: " << batch->first_instance << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::Draw]   instance_count: " << batch->instance_count << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::Draw]   geom_data_buffer: " << (void*)batch->geom_data_buffer << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::Draw]   geom_draw_range: " << (void*)batch->geom_draw_range << std::endl;
        
        if (batch->geom_data_buffer)
        {
            std::cout << "[ECSPipelineMaterialRenderer::Draw]   DataBuffer.vdm: " << (void*)batch->geom_data_buffer->vdm << std::endl;
            std::cout << "[ECSPipelineMaterialRenderer::Draw]   DataBuffer.ibo: " << batch->geom_data_buffer->ibo << std::endl;
        }
        
        // 检查是否需要切换几何数据缓冲
        const bool need_buffer_switch = !last_data_buffer || 
                                       *(batch->geom_data_buffer) != *last_data_buffer;

        std::cout << "[ECSPipelineMaterialRenderer::Draw] need_buffer_switch: " << need_buffer_switch << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::Draw] last_data_buffer: " << (void*)last_data_buffer << std::endl;

        if (need_buffer_switch)
        {
            std::cout << "[ECSPipelineMaterialRenderer::Draw] Buffer switch required!" << std::endl;
            
            // 先提交之前累积的间接绘制
            if (indirect_draw_count)
            {
                std::cout << "[ECSPipelineMaterialRenderer::Draw] Flushing " << indirect_draw_count << " pending indirect draws..." << std::endl;
                ProcIndirectRender(icb_draw, icb_draw_indexed);
            }

            // 更新缓冲状态
            last_data_buffer = batch->geom_data_buffer;
            last_draw_range = nullptr;

            std::cout << "[ECSPipelineMaterialRenderer::Draw] Binding VAB..." << std::endl;
            // 绑定新的顶点数组缓冲
            if (!BindVAB(batch, transform_buffer, mi_buffer))
            {
                std::cout << "[ECSPipelineMaterialRenderer::Draw] ERROR: BindVAB failed!" << std::endl;
                return false;
            }
            std::cout << "[ECSPipelineMaterialRenderer::Draw] VAB bound successfully" << std::endl;

            // 如果有索引缓冲，也需要绑定
            if (batch->geom_data_buffer->ibo)
            {
                std::cout << "[ECSPipelineMaterialRenderer::Draw] Binding IBO: " << batch->geom_data_buffer->ibo << std::endl;
                cmd_buf->BindIBO(batch->geom_data_buffer->ibo);
                std::cout << "[ECSPipelineMaterialRenderer::Draw] IBO bound" << std::endl;
            }
            else
            {
                std::cout << "[ECSPipelineMaterialRenderer::Draw] No IBO to bind" << std::endl;
            }
        }
        else
        {
            std::cout << "[ECSPipelineMaterialRenderer::Draw] Using cached buffer (no switch)" << std::endl;
        }

        // 提交绘制命令
        if (batch->geom_data_buffer->vdm)
        {
            std::cout << "[ECSPipelineMaterialRenderer::Draw] Using INDIRECT draw (has VDM)" << std::endl;
            // 间接绘制：累积命令
            if (indirect_draw_count == 0)
            {
                first_indirect_draw_index = batch->first_instance;
                std::cout << "[ECSPipelineMaterialRenderer::Draw] Starting new indirect batch at index " << first_indirect_draw_index << std::endl;
            }

            ++indirect_draw_count;
            std::cout << "[ECSPipelineMaterialRenderer::Draw] Accumulated indirect draw count: " << indirect_draw_count << std::endl;
        }
        else
        {
            std::cout << "[ECSPipelineMaterialRenderer::Draw] Using DIRECT draw (no VDM)" << std::endl;
            std::cout << "[ECSPipelineMaterialRenderer::Draw] Draw parameters:" << std::endl;
            std::cout << "[ECSPipelineMaterialRenderer::Draw]   instance_count: " << batch->instance_count << std::endl;
            std::cout << "[ECSPipelineMaterialRenderer::Draw]   first_instance: " << batch->first_instance << std::endl;
            
            // 直接绘制：立即提交
            cmd_buf->Draw(batch->geom_data_buffer, batch->geom_draw_range, 
                         batch->instance_count, batch->first_instance);
            std::cout << "[ECSPipelineMaterialRenderer::Draw] Direct draw submitted" << std::endl;
        }

        std::cout << "[ECSPipelineMaterialRenderer::Draw] === EXIT (success) ===" << std::endl;
        return true;
    }

    void ECSPipelineMaterialRenderer::Render(graph::RenderCmdBuffer* rcb,
                                              const graph::DrawBatchArray& batches,
                                              uint32_t batch_count,
                                              ECSTransformAssignmentBuffer* transform_buffer,
                                              ECSMaterialInstanceAssignmentBuffer* mi_buffer,
                                              graph::IndirectDrawBuffer* icb_draw,
                                              graph::IndirectDrawIndexedBuffer* icb_draw_indexed)
    {
        std::cout << "[ECSPipelineMaterialRenderer::Render] === ENTRY ===" << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::Render] RenderCmdBuffer: " << (void*)rcb << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::Render] Batch count: " << batch_count << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::Render] Material: " << (void*)material << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::Render] Pipeline: " << (void*)pipeline << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::Render] ECS Transform buffer: " << (void*)transform_buffer << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::Render] ECS MI buffer: " << (void*)mi_buffer << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::Render] ICB Draw: " << (void*)icb_draw << std::endl;
        std::cout << "[ECSPipelineMaterialRenderer::Render] ICB Draw Indexed: " << (void*)icb_draw_indexed << std::endl;
        
        // 前置条件检查
        if (!rcb)
        {
            std::cout << "[ECSPipelineMaterialRenderer::Render] ERROR: No render command buffer!" << std::endl;
            return;
        }
        if (batch_count <= 0)
        {
            std::cout << "[ECSPipelineMaterialRenderer::Render] WARNING: No batches to render!" << std::endl;
            return;
        }

        cmd_buf = rcb;

        // 绑定管线
        std::cout << "[ECSPipelineMaterialRenderer::Render] Binding pipeline..." << std::endl;
        cmd_buf->BindPipeline(pipeline);
        std::cout << "[ECSPipelineMaterialRenderer::Render] Pipeline bound" << std::endl;

        // 重置渲染状态缓存
        last_data_buffer = nullptr;
        last_vdm = nullptr;
        last_draw_range = nullptr;
        indirect_draw_count = 0;
        first_indirect_draw_index = -1;
        std::cout << "[ECSPipelineMaterialRenderer::Render] Render state cache reset" << std::endl;

        // 绑定ECS Transform分配缓冲（如果有）
        if (transform_buffer)
        {
            std::cout << "[ECSPipelineMaterialRenderer::Render] Binding ECS transform buffer..." << std::endl;
            transform_buffer->BindTransform(material);
            std::cout << "[ECSPipelineMaterialRenderer::Render] ECS Transform buffer bound" << std::endl;
        }
        
        // 绑定ECS MaterialInstance分配缓冲（如果有）
        if (mi_buffer)
        {
            std::cout << "[ECSPipelineMaterialRenderer::Render] Binding ECS material instance buffer..." << std::endl;
            mi_buffer->BindMaterialInstance(material);
            std::cout << "[ECSPipelineMaterialRenderer::Render] ECS Material instance buffer bound" << std::endl;
        }

        // 绑定材质描述符集
        std::cout << "[ECSPipelineMaterialRenderer::Render] Binding material descriptor sets..." << std::endl;
        cmd_buf->BindDescriptorSets(material);
        std::cout << "[ECSPipelineMaterialRenderer::Render] Descriptor sets bound" << std::endl;

        // 遍历绘制批次
        graph::DrawBatch* batch = const_cast<graph::DrawBatch*>(batches.GetData());
        std::cout << "[ECSPipelineMaterialRenderer::Render] Processing " << batch_count << " batches..." << std::endl;
        
        for (uint32_t i = 0; i < batch_count; i++)
        {
            std::cout << "[ECSPipelineMaterialRenderer::Render] === Processing batch " << i << " ===" << std::endl;
            Draw(batch, transform_buffer, mi_buffer, icb_draw, icb_draw_indexed);
            ++batch;
        }
        
        // 提交剩余的间接绘制命令
        if (indirect_draw_count)
        {
            std::cout << "[ECSPipelineMaterialRenderer::Render] Flushing remaining " << indirect_draw_count << " indirect draws..." << std::endl;
            ProcIndirectRender(icb_draw, icb_draw_indexed);
        }
        
        std::cout << "[ECSPipelineMaterialRenderer::Render] === EXIT ===" << std::endl;
    }
}//namespace hgl::ecs
