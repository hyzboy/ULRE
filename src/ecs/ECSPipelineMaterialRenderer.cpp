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
    }

    ECSPipelineMaterialRenderer::~ECSPipelineMaterialRenderer()
    {
        SAFE_CLEAR(vab_list);
    }

    bool ECSPipelineMaterialRenderer::BindVAB(const graph::DrawBatch* batch, 
                                               ECSTransformAssignmentBuffer* transform_buffer, 
                                               ECSMaterialInstanceAssignmentBuffer* mi_buffer)
    {
        // Log GeometryDataBuffer details
        if (batch->geom_data_buffer)
        {
            // Log each VAB
            for (uint32_t i = 0; i < batch->geom_data_buffer->vab_count; i++)
            {
                std::cout << "[ECSPipelineMaterialRenderer::BindVAB]   VAB[" << i << "]: buffer=" 
                          << batch->geom_data_buffer->vab_list[i] 
                          << ", offset=" << batch->geom_data_buffer->vab_offset[i] << std::endl;
            }
        }

        vab_list->Restart();

        // 添加几何数据的VAB
        if (!vab_list->Add(batch->geom_data_buffer))
        {
            std::cout << "[ECSPipelineMaterialRenderer::BindVAB] ERROR: Failed to add geometry data buffer to VABList!" << std::endl;
            return false;
        }

        // 如果有ECS Transform分配缓冲，绑定Transform索引VAB
        if (transform_buffer)
        {
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
            }
        }

        // 如果有ECS MaterialInstance分配缓冲，绑定MaterialInstance索引VAB
        if (mi_buffer)
        {
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
            }
        }

        cmd_buf->BindVAB(vab_list);

        return true;
    }

    void ECSPipelineMaterialRenderer::ProcIndirectRender(graph::IndirectDrawBuffer* icb_draw, 
                                                         graph::IndirectDrawIndexedBuffer* icb_draw_indexed)
    {
        // 提交累积的间接绘制命令
        if (last_data_buffer->ibo)
        {
            icb_draw_indexed->DrawIndexed(*cmd_buf, first_indirect_draw_index, indirect_draw_count);
        }
        else
        {
            icb_draw->Draw(*cmd_buf, first_indirect_draw_index, indirect_draw_count);
        }

        // 重置间接绘制状态
        first_indirect_draw_index = -1;
        indirect_draw_count = 0;
    }

    bool ECSPipelineMaterialRenderer::Draw(graph::DrawBatch* batch,
                                            ECSTransformAssignmentBuffer* transform_buffer,
                                            ECSMaterialInstanceAssignmentBuffer* mi_buffer,
                                            graph::IndirectDrawBuffer* icb_draw,
                                            graph::IndirectDrawIndexedBuffer* icb_draw_indexed)
    {
        // if (batch->geom_data_buffer)
        // {
        //     std::cout << "[ECSPipelineMaterialRenderer::Draw]   DataBuffer.vdm: " << (void*)batch->geom_data_buffer->vdm << std::endl;
        //     std::cout << "[ECSPipelineMaterialRenderer::Draw]   DataBuffer.ibo: " << batch->geom_data_buffer->ibo << std::endl;
        // }

        // 检查是否需要切换几何数据缓冲
        const bool need_buffer_switch = !last_data_buffer || 
                                       *(batch->geom_data_buffer) != *last_data_buffer;

        if (need_buffer_switch)
        {
            // 先提交之前累积的间接绘制
            if (indirect_draw_count)
            {
                ProcIndirectRender(icb_draw, icb_draw_indexed);
            }

            // 更新缓冲状态
            last_data_buffer = batch->geom_data_buffer;
            last_draw_range = nullptr;

            // 绑定新的顶点数组缓冲
            if (!BindVAB(batch, transform_buffer, mi_buffer))
            {
                std::cout << "[ECSPipelineMaterialRenderer::Draw] ERROR: BindVAB failed!" << std::endl;
                return false;
            }

            // 如果有索引缓冲，也需要绑定
            if (batch->geom_data_buffer->ibo)
            {
                cmd_buf->BindIBO(batch->geom_data_buffer->ibo);
            }
            // else
            // {
            //     std::cout << "[ECSPipelineMaterialRenderer::Draw] No IBO to bind" << std::endl;
            // }
        }
        // else
        // {
        //     std::cout << "[ECSPipelineMaterialRenderer::Draw] Using cached buffer (no switch)" << std::endl;
        // }

        // 提交绘制命令
        if (batch->geom_data_buffer->vdm)
        {
            // 间接绘制：累积命令
            if (indirect_draw_count == 0)
            {
                first_indirect_draw_index = batch->first_instance;
            }

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

    void ECSPipelineMaterialRenderer::Render(graph::RenderCmdBuffer* rcb,
                                              const graph::DrawBatchArray& batches,
                                              uint32_t batch_count,
                                              ECSTransformAssignmentBuffer* transform_buffer,
                                              ECSMaterialInstanceAssignmentBuffer* mi_buffer,
                                              graph::IndirectDrawBuffer* icb_draw,
                                              graph::IndirectDrawIndexedBuffer* icb_draw_indexed)
    {
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
        cmd_buf->BindPipeline(pipeline);

        // 重置渲染状态缓存
        last_data_buffer = nullptr;
        last_vdm = nullptr;
        last_draw_range = nullptr;
        indirect_draw_count = 0;
        first_indirect_draw_index = -1;

        // 绑定ECS Transform分配缓冲（如果有）
        if (transform_buffer)
        {
            transform_buffer->BindTransform(material);
        }

        // 绑定ECS MaterialInstance分配缓冲（如果有）
        if (mi_buffer)
        {
            mi_buffer->BindMaterialInstance(material);
        }

        // 绑定材质描述符集
        cmd_buf->BindDescriptorSets(material);

        // 遍历绘制批次
        graph::DrawBatch* batch = const_cast<graph::DrawBatch*>(batches.GetData());

        for (uint32_t i = 0; i < batch_count; i++)
        {
            Draw(batch, transform_buffer, mi_buffer, icb_draw, icb_draw_indexed);
            ++batch;
        }

        // 提交剩余的间接绘制命令
        if (indirect_draw_count)
        {
            ProcIndirectRender(icb_draw, icb_draw_indexed);
        }
    }
}//namespace hgl::ecs
