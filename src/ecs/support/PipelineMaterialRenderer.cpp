/**
 * PipelineMaterialRenderer.cpp - ECS Pipeline材质渲染器实现
 *
 * 参照 PipelineMaterialRenderer 实现，但使用 ECS 版本的 Assignment Buffers
 */

#include<hgl/ecs/support/PipelineMaterialRenderer.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/log/Log.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKVertexInput.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/vk/VKIndirectCommandBuffer.h>
#include<hgl/vk/VKBindlessTextureManager.h>

namespace hgl::ecs
{
    PipelineMaterialRenderer::PipelineMaterialRenderer(graph::ShaderProgram* m, graph::Pipeline* p)
        : material(m)
        , pipeline(p)
        , cmd_buf(nullptr)
        , vab_list(new graph::VABList(m->GetVertexInput()->GetCount()))
        , last_data_buffer(nullptr)
        , first_indirect_draw_index(-1)
        , indirect_draw_count(0)
    {
    }

    PipelineMaterialRenderer::~PipelineMaterialRenderer()
    {
        SAFE_CLEAR(vab_list);
    }

    bool PipelineMaterialRenderer::BindVAB(const DrawBatch* batch)
    {
        vab_list->Restart();

        // 添加几何数据的VAB
        if (!vab_list->Add(batch->geom_data_buffer))
        {
            GLogError("[PipelineMaterialRenderer::BindVAB] Failed to add geometry data buffer to VABList");
            return false;
        }

        if (!vab_list->IsFull())
        {
            GLogWarning("[PipelineMaterialRenderer::BindVAB] VABList not full (%u/%u), padding with VK_NULL_HANDLE",
                        vab_list->GetWriteCount(),
                        material->GetVertexInput()->GetCount());

            while (!vab_list->IsFull())
            {
                vab_list->Add(VK_NULL_HANDLE, 0);
            }
        }

        cmd_buf->BindVAB(vab_list);

        return true;
    }

    void PipelineMaterialRenderer::ProcIndirectRender(graph::IndirectDrawBuffer* icb_draw,
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

    bool PipelineMaterialRenderer::Draw( DrawBatch* batch,
                                            TransformAssignmentBuffer* transform_buffer,
                                            graph::IndirectDrawBuffer* icb_draw,
                                            graph::IndirectDrawIndexedBuffer* icb_draw_indexed)
    {
        (void)transform_buffer;

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

            // 绑定新的顶点数组缓冲
            if (!BindVAB(batch))
            {
                GLogError("[PipelineMaterialRenderer::Draw] BindVAB failed");
                return false;
            }

            // 如果有索引缓冲，也需要绑定
            if (batch->geom_data_buffer->ibo)
            {
                cmd_buf->BindIBO(batch->geom_data_buffer->ibo);
            }
        }

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

    void PipelineMaterialRenderer::Render(graph::RenderCmdBuffer* rcb,
                                              const DrawBatchArray& batches,
                                              uint32_t batch_count,
                                              TransformAssignmentBuffer* transform_buffer,
                                              graph::IndirectDrawBuffer* icb_draw,
                                              graph::IndirectDrawIndexedBuffer* icb_draw_indexed,
                                              const MaterialBatch *owner_batch,
                                              graph::RenderContext *render_context)
    {
        // 前置条件检查
        if (!rcb)
        {
            GLogError("[PipelineMaterialRenderer::Render] No render command buffer");
            return;
        }

        if (batch_count <= 0)
            return;

        cmd_buf = rcb;

        // 绑定管线
        cmd_buf->BindPipeline(pipeline);

        // 重置渲染状态缓存
        last_data_buffer = nullptr;
        indirect_draw_count = 0;
        first_indirect_draw_index = -1;

        // L2W / MI descriptor binding is unified in RenderDescriptorBindingSystem.
        // PipelineMaterialRenderer only handles VAB/IBO and draw submission here.
        if (!material->hasLocalToWorld())
        {
            transform_buffer=nullptr;
        }

        if (owner_batch && owner_batch->has_batch_descriptor_overrides)
        {
            const VkPipelineLayout layout = material->GetPipelineLayout();
            for (uint32_t set_index = 0; set_index < graph::DESCRIPTOR_SET_TYPE_COUNT; ++set_index)
            {
                auto *mp = owner_batch->batch_descriptor_mp[set_index];
                if (!mp)
                    mp = material->GetMP(static_cast<graph::DescriptorSetType>(set_index));
                if (!mp)
                    continue;

                mp->Update();
                const VkDescriptorSet ds = mp->GetVkDescriptorSet();
                cmd_buf->BindDescriptorSets(layout, set_index, &ds, 1, nullptr, 0);
            }
        }
        else
        {
            // 绑定材质描述符集
            cmd_buf->BindDescriptorSets(material);
        }

        if (render_context)
        {
            auto *bindless_mgr = render_context->GetManager<graph::BindlessTextureManager>();
            if (bindless_mgr && bindless_mgr->IsValid())
            {
                bool needs_bindless_set = false;
                const auto &contract = material->GetShaderResourceSchema();
                for (const auto &req : contract.resources)
                {
                    if (req.semantic == graph::mtl::DescriptorSemantic::MaterialTextureLayerTable)
                    {
                        needs_bindless_set = true;
                        break;
                    }
                }

                if (needs_bindless_set)
                {
                    constexpr uint32_t bindless_set = static_cast<uint32_t>(graph::DescriptorSetType::Bindless);
                    bindless_mgr->BindToCmd(*cmd_buf, material->GetPipelineLayout(), bindless_set);
                }
            }
        }

        // 遍历绘制批次
        DrawBatch* batch = const_cast<DrawBatch*>(batches.data());

        for (uint32_t i = 0; i < batch_count; i++)
        {
            Draw(batch, transform_buffer, icb_draw, icb_draw_indexed);
            ++batch;
        }

        // 提交剩余的间接绘制命令
        if (indirect_draw_count)
        {
            ProcIndirectRender(icb_draw, icb_draw_indexed);
        }
    }
}//namespace hgl::ecs
