/**
 * PipelineMaterialRenderer.cpp - ECS Pipeline材质渲染器实现
 *
 * 参照 PipelineMaterialRenderer 实现，但使用 ECS 版本的 Assignment Buffers
 */

#include<hgl/ecs/support/PipelineMaterialRenderer.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/ecs/support/MaterialInstanceAssignmentBuffer.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKVertexInput.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKIndirectCommandBuffer.h>
#include<hgl/vk/VKDomainResourceBinding.h>
#include<iostream>

namespace hgl::ecs
{
    namespace
    {
        uint32_t GetMaterialVertexInputCount(const graph::ShaderMaterialProgram *m)
        {
            if (!m)
                return 0;

            const auto *vi = m->GetVertexInput();
            if (!vi)
                return 0;

            return vi->GetCount();
        }
    }

    PipelineMaterialRenderer::PipelineMaterialRenderer(graph::ShaderMaterialProgram* m, graph::GraphicsPipeline* p)
        : material(m)
        , pipeline(p)
        , cmd_buf(nullptr)
        , vab_list(new graph::VABList(GetMaterialVertexInputCount(m)))
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

    bool PipelineMaterialRenderer::BindVAB(const DrawBatch* batch,
                                               MaterialInstanceAssignmentBuffer* mi_buffer)
    {
        if (material)
        {
            const bool pulling_enabled =
                material->IsPullingEnabled() || material->hasSet(graph::DescriptorSetType::VertexStreams);

            if (pulling_enabled)
            {
                (void)batch;
                (void)mi_buffer;
                return true;
            }
        }

        // Log GeometryDataBuffer details
        //if (batch->geom_data_buffer)
        //{
        //    // Log each VAB
        //    for (uint32_t i = 0; i < batch->geom_data_buffer->vab_count; i++)
        //    {
        //        std::cout << "[PipelineMaterialRenderer::BindVAB]   VAB[" << i << "]: buffer="
        //                  << batch->geom_data_buffer->vab_list[i]
        //                  << ", offset=" << batch->geom_data_buffer->vab_offset[i] << std::endl;
        //    }
        //}

        vab_list->Restart();

        // 添加几何数据的VAB。
        // PCG类材质（如FullscreenTriangle）的VIL attr_count=0，vab_list容量为0，
        // Restart()后IsFull()即为true，直接跳过Add，不需要也不应绑定任何顶点缓冲。
        if (!vab_list->IsFull())
        {
            if (!vab_list->Add(batch->geom_data_buffer))
            {
                std::cout << "[PipelineMaterialRenderer::BindVAB] ERROR: Failed to add geometry data buffer to VABList!" << std::endl;
                return false;
            }

            if (!vab_list->IsFull())
            {
                std::cout << "[PipelineMaterialRenderer::BindVAB] WARNING: VABList not full ("
                          << vab_list->GetWriteCount() << "/"
                          << GetMaterialVertexInputCount(material)
                          << "), padding with VK_NULL_HANDLE" << std::endl;

                while (!vab_list->IsFull())
                {
                    vab_list->Add(VK_NULL_HANDLE, 0);
                }
            }
        }

        (void)mi_buffer;

        cmd_buf->BindVAB(vab_list);

        return true;
    }

    void PipelineMaterialRenderer::ProcIndirectRender(graph::IndirectDrawBuffer* icb_draw,
                                                         graph::IndirectDrawIndexedBuffer* icb_draw_indexed)
    {
        if (cmd_buf && cmd_buf->IsBoundMeshPipeline())
        {
            std::cout << "[PipelineMaterialRenderer::ProcIndirectRender] WARNING: skip indirect draw flush in mesh pipeline mode" << std::endl;
            first_indirect_draw_index = -1;
            indirect_draw_count = 0;
            return;
        }

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

    bool PipelineMaterialRenderer::Draw( DrawBatch* batch,
                                            TransformAssignmentBuffer* transform_buffer,
                                            MaterialInstanceAssignmentBuffer* mi_buffer,
                                            graph::IndirectDrawBuffer* icb_draw,
                                            graph::IndirectDrawIndexedBuffer* icb_draw_indexed)
    {
        // if (batch->geom_data_buffer)
        // {
        //     std::cout << "[PipelineMaterialRenderer::Draw]   DataBuffer.vdm: " << (void*)batch->geom_data_buffer->vdm << std::endl;
        //     std::cout << "[PipelineMaterialRenderer::Draw]   DataBuffer.ibo: " << batch->geom_data_buffer->ibo << std::endl;
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
            if (!BindVAB(batch, mi_buffer))
            {
                std::cout << "[PipelineMaterialRenderer::Draw] ERROR: BindVAB failed!" << std::endl;
                return false;
            }

            // 如果有索引缓冲，也需要绑定
            if (batch->geom_data_buffer->ibo && !(cmd_buf && cmd_buf->IsBoundMeshPipeline()))
            {
                cmd_buf->BindIBO(batch->geom_data_buffer->ibo);
            }
            // else
            // {
            //     std::cout << "[PipelineMaterialRenderer::Draw] No IBO to bind" << std::endl;
            // }
        }
        // else
        // {
        //     std::cout << "[PipelineMaterialRenderer::Draw] Using cached buffer (no switch)" << std::endl;
        // }

        const bool mesh_pipeline_active = cmd_buf && cmd_buf->IsBoundMeshPipeline();

        // 提交绘制命令
        if (batch->geom_data_buffer->vdm && !mesh_pipeline_active)
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

    void PipelineMaterialRenderer::Render(graph::RenderCmdBuffer* rcb,
                                              const DrawBatchArray& batches,
                                              uint32_t batch_count,
                                              TransformAssignmentBuffer* transform_buffer,
                                              MaterialInstanceAssignmentBuffer* mi_buffer,
                                              graph::IndirectDrawBuffer* icb_draw,
                                              graph::IndirectDrawIndexedBuffer* icb_draw_indexed,
                                              graph::DomainResourceBinding* domain_binding,
                                              bool skip_pipeline_bind)
    {
        // 前置条件检查
        if (!rcb)
        {
            std::cout << "[PipelineMaterialRenderer::Render] ERROR: No render command buffer!" << std::endl;
            return;
        }

        if (batch_count <= 0)
        {
            std::cout << "[PipelineMaterialRenderer::Render] WARNING: No batches to render!" << std::endl;
            return;
        }

        cmd_buf = rcb;

        // 绑定管线（排序后相邻同 pipeline 的批次可跳过）
        if (!skip_pipeline_bind)
            cmd_buf->BindPipeline(pipeline);

        // 重置渲染状态缓存
        last_data_buffer = nullptr;
        last_vdm = nullptr;
        last_draw_range = nullptr;
        indirect_draw_count = 0;
        first_indirect_draw_index = -1;

        // L2W / MI descriptor binding is unified in RenderDescriptorBindingSystem.
        // PipelineMaterialRenderer only handles VAB/IBO and draw submission here.
        if (!material->hasLocalToWorld())
        {
            transform_buffer=nullptr;
        }

        // 绑定材质描述符集（优先 domain+material 绑定，回退旧 material 绑定）
        if (domain_binding)
            cmd_buf->BindDescriptorSets(domain_binding);
        else
            cmd_buf->BindDescriptorSets(material);

        // 遍历绘制批次
        DrawBatch* batch = const_cast<DrawBatch*>(batches.data());

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

