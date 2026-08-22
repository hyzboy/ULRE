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
#include<hgl/vk/VKGlobalSceneUBOSet.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
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
        , last_data_buffer(nullptr)
        , first_indirect_draw_index(-1)
        , indirect_draw_count(0)
    {
    }

    PipelineMaterialRenderer::~PipelineMaterialRenderer()
    {
    }

    void PipelineMaterialRenderer::ProcIndirectRender(graph::IndirectDrawBuffer* icb_draw)
    {
        // 提交累积的间接绘制命令（SSBO 顶点输入：统一非索引间接——索引数据走 sbo_index）
        icb_draw->Draw(*cmd_buf, first_indirect_draw_index, indirect_draw_count);

        // 重置间接绘制状态（命令序号累计到本批次已提交段）
        first_indirect_draw_index = -1;
        indirect_draw_command_offset += indirect_draw_count;
        indirect_draw_count = 0;
    }

    bool PipelineMaterialRenderer::Draw( DrawBatch* batch,
                                            TransformAssignmentBuffer* transform_buffer,
                                            graph::IndirectDrawBuffer* icb_draw,
                                            const MaterialBatch *owner_batch)
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
                ProcIndirectRender(icb_draw);
            }

            // 更新缓冲状态
            last_data_buffer = batch->geom_data_buffer;

            // 绑定新的顶点数组缓冲（SSBO 顶点输入材质管线无顶点输入——VBO 绑定已删除）

            // SSBO 顶点输入：把当前 DrawBatch 的顶点 buffer 绑到顶点 SSBO 槽
            //（RDBS 帧前按 batch 首对象绑定——独立 VAB 多对象时其余对象 buffer 错位；
            //  此处 per-DrawBatch 重绑——VDM 共享 buffer 同值缓存跳过，独立 VAB 每对象正确）
            if (ssbo_vertex_input)
            {
                bool changed = false;
                const auto *geom_buffer = batch->geom_data_buffer;

                // 按 VAB 自带语义遍历（GeometryDataBuffer::Update 按 VIF 填充——
                // 不依赖 material 的 VIL，独立 VAB/VDM 场景均正确）
                for (uint32_t vi = 0; vi < geom_buffer->vab_count; ++vi)
                {
                    const VkBuffer buf = geom_buffer->vab_list[vi];
                    if (!buf)
                        continue;

                    switch (geom_buffer->vab_semantic[vi])
                    {
                    case graph::VertexSemantic::Position:
                        if (buf != last_ssbo_pos)
                        {
                            material->BindSSBO(graph::DescriptorSetType::PerObject,
                                               "VertexPosition", buf, 0, VK_WHOLE_SIZE);
                            last_ssbo_pos = buf;
                            changed = true;
                        }
                        break;
                    case graph::VertexSemantic::TexCoord:
                        if (buf != last_ssbo_uv)
                        {
                            material->BindSSBO(graph::DescriptorSetType::PerObject,
                                               "VertexUV", buf, 0, VK_WHOLE_SIZE);
                            last_ssbo_uv = buf;
                            changed = true;
                        }
                        break;
                    case graph::VertexSemantic::Normal:
                        if (buf != last_ssbo_ntb)
                        {
                            material->BindSSBO(graph::DescriptorSetType::PerObject,
                                               "VertexNTB", buf, 0, VK_WHOLE_SIZE);
                            last_ssbo_ntb = buf;
                            changed = true;
                        }
                        break;
                    case graph::VertexSemantic::Color:
                        if (buf != last_ssbo_color)
                        {
                            material->BindSSBO(graph::DescriptorSetType::PerObject,
                                               "VertexColor", buf, 0, VK_WHOLE_SIZE);
                            last_ssbo_color = buf;
                            changed = true;
                        }
                        break;
                    case graph::VertexSemantic::Luminance:
                        if (buf != last_ssbo_luminance)
                        {
                            material->BindSSBO(graph::DescriptorSetType::PerObject,
                                               "VertexLuminance", buf, 0, VK_WHOLE_SIZE);
                            last_ssbo_luminance = buf;
                            changed = true;
                        }
                        break;
                    case graph::VertexSemantic::TransformID:
                        if (buf != last_ssbo_transform_id)
                        {
                            material->BindSSBO(graph::DescriptorSetType::PerObject,
                                               "VertexTransformID", buf, 0, VK_WHOLE_SIZE);
                            last_ssbo_transform_id = buf;
                            changed = true;
                        }
                        break;
                    default: break;
                    }
                }

                // geometry 直取（渲染路径的 geom_data_buffer 无 VAB 数据
                // （vab_count=0——PrimitiveComponent runtime buffer）时的补绑）
                if (batch->geometry && geom_buffer->vab_count == 0)
                {
                    if (auto *vab = batch->geometry->GetVAB(graph::VertexSemantic::Position))
                    {
                        const VkBuffer buf = vab->GetVkBuffer();
                        if (buf != last_ssbo_pos)
                        {
                            material->BindSSBO(graph::DescriptorSetType::PerObject,
                                               "VertexPosition", buf, 0, VK_WHOLE_SIZE);
                            last_ssbo_pos = buf;
                            changed = true;
                        }
                    }
                    if (auto *vab = batch->geometry->GetVAB(graph::VertexSemantic::TexCoord))
                    {
                        const VkBuffer buf = vab->GetVkBuffer();
                        if (buf != last_ssbo_uv)
                        {
                            material->BindSSBO(graph::DescriptorSetType::PerObject,
                                               "VertexUV", buf, 0, VK_WHOLE_SIZE);
                            last_ssbo_uv = buf;
                            changed = true;
                        }
                    }
                    if (auto *vab = batch->geometry->GetVAB(graph::VertexSemantic::Normal))
                    {
                        const VkBuffer buf = vab->GetVkBuffer();
                        if (buf != last_ssbo_ntb)
                        {
                            material->BindSSBO(graph::DescriptorSetType::PerObject,
                                               "VertexNTB", buf, 0, VK_WHOLE_SIZE);
                            last_ssbo_ntb = buf;
                            changed = true;
                        }
                    }
                    if (auto *vab = batch->geometry->GetVAB(graph::VertexSemantic::Color))
                    {
                        const VkBuffer buf = vab->GetVkBuffer();
                        if (buf != last_ssbo_color)
                        {
                            material->BindSSBO(graph::DescriptorSetType::PerObject,
                                               "VertexColor", buf, 0, VK_WHOLE_SIZE);
                            last_ssbo_color = buf;
                            changed = true;
                        }
                    }
                    if (auto *vab = batch->geometry->GetVAB(graph::VertexSemantic::Luminance))
                    {
                        const VkBuffer buf = vab->GetVkBuffer();
                        if (buf != last_ssbo_luminance)
                        {
                            material->BindSSBO(graph::DescriptorSetType::PerObject,
                                               "VertexLuminance", buf, 0, VK_WHOLE_SIZE);
                            last_ssbo_luminance = buf;
                            changed = true;
                        }
                    }
                }

                if (changed)
                {
                    auto *mp = material->GetMP(graph::DescriptorSetType::PerObject);
                    if (mp)
                    {
                        mp->Update();
                        const VkDescriptorSet ds = mp->GetVkDescriptorSet();
                        cmd_buf->BindDescriptorSets(material->GetPipelineLayout(),
                                                    static_cast<uint32_t>(graph::DescriptorSetType::PerObject),
                                                    &ds, 1, nullptr, 0);
                    }
                }

                // 顶点索引 SSBO（非索引绘制——索引数据统一 SSBO）：
                // IBO buffer 绑 VertexIndex 槽（VDM 共享/独立 IBO 均正确——段偏移走 push constant）
                if (geom_buffer->ibo)
                {
                    const VkBuffer ibuf = geom_buffer->ibo->GetVkBuffer();
                    if (ibuf != last_ssbo_index)
                    {
                        material->BindSSBO(graph::DescriptorSetType::PerObject,
                                           "VertexIndex", ibuf, 0, VK_WHOLE_SIZE);
                        last_ssbo_index = ibuf;

                        auto *mp = material->GetMP(graph::DescriptorSetType::PerObject);
                        if (mp)
                        {
                            mp->Update();
                            const VkDescriptorSet ds = mp->GetVkDescriptorSet();
                            cmd_buf->BindDescriptorSets(material->GetPipelineLayout(),
                                                        static_cast<uint32_t>(graph::DescriptorSetType::PerObject),
                                                        &ds, 1, nullptr, 0);
                        }
                    }
                }

                // l2w / index rows 补绑（独立 VAB 场景的 program 实例可能没有
                // RDBS 预绑——Draw 侧统一补到 material 的 PerObject MP）
                if (transform_buffer)
                {
                    transform_buffer->BindTransform(material);
                    changed = true;
                }
                if (owner_batch)
                {
                    if (owner_batch->l2w_index_rows_buffer)
                    {
                        material->BindSSBO(graph::DescriptorSetType::PerObject,
                                           "l2w_index_rows",
                                           owner_batch->l2w_index_rows_buffer->GetGPUBuffer());
                        changed = true;
                    }
                    if (owner_batch->material_data_index_rows_buffer)
                    {
                        material->BindSSBO(graph::DescriptorSetType::PerObject,
                                           "mtl_data_index_rows",
                                           owner_batch->material_data_index_rows_buffer->GetGPUBuffer());
                        changed = true;
                    }
                }

                if (changed)
                {
                    auto *mp = material->GetMP(graph::DescriptorSetType::PerObject);
                    if (mp)
                    {
                        mp->Update();
                        const VkDescriptorSet ds = mp->GetVkDescriptorSet();
                        cmd_buf->BindDescriptorSets(material->GetPipelineLayout(),
                                                    static_cast<uint32_t>(graph::DescriptorSetType::PerObject),
                                                    &ds, 1, nullptr, 0);
                    }
                }
            }
        }

        // per-draw 段偏移 push constant（每 DrawBatch 必推——VDM 共享 buffer 各模型
        // 段偏移不同；间接累积提交无法 per-draw——SSBO 材质直接绘制）
        if (ssbo_vertex_input)
        {
            const uint32_t pc_data[3] = {
                static_cast<uint32_t>(batch->geom_draw_range->first_index),
                static_cast<uint32_t>(batch->geom_draw_range->vertex_offset),
                batch->geom_draw_range->index_count > 0 ? 1u : 0u    // is_indexed：几何有 IBO 走索引绘制（查表）
            };
            cmd_buf->PushConstants(material->GetPipelineLayout(), pc_data, sizeof(pc_data));
        }

        // 提交绘制命令
        if (batch->geom_data_buffer->vdm && !ssbo_vertex_input)
        {
            // 间接绘制：累积命令。命令偏移取本批次 ICB 的命令序号累计，
            // 不能用 first_instance（实例索引）——vdm/非 vdm 混排时二者脱节，
            // 会读取到未写入/错误的 ICB 命令。
            // 注：SSBO 顶点输入材质不走间接——段偏移（index_base/vertex_base）
            // 经 push constant per-draw 传递，间接累积提交时只有最后一条命令
            // 的 push constant 生效（多段偏移冲突）；直接绘制 per-draw 正确。
            if (indirect_draw_count == 0)
            {
                first_indirect_draw_index =
                    static_cast<int32_t>(indirect_draw_command_offset);
            }

            ++indirect_draw_count;
        }
        else
        {
            // 直接绘制：立即提交（SSBO 顶点输入统一非索引绘制——索引数据走
            // 顶点索引 SSBO，段偏移在 shader 内经 push constant 定位）
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

        // Set 0（Scene UBO）/ Set 3（Bindless 纹理）按材质自身 layout 绑定。
        // VVL 的 set 兼容 ID 取 layout 在 set 0..N 的全部 DSL 前缀，绑定 layout 必须与
        // draw 时管线 layout（= 材质 pipeline layout）一致。旧方案由
        // RenderDescriptorBindingSystem 用"第一个活跃材质"的 layout 统一绑定，
        // 会导致使用 bindless 纹理的材质触发 set 兼容性 VUID
        //（其 set 0..3 前缀 DSL 与绑定 layout 不同，set 3 被判为不兼容）。
        if (render_context)
        {
            if (auto *gc = render_context->GetGraphicsContext())
            {
                const VkPipelineLayout layout = material->GetPipelineLayout();

                if (auto *scene_set = gc->GetGlobalSceneUBOSet();
                    scene_set && scene_set->IsValid())
                {
                    scene_set->BindToCmd(*cmd_buf, layout);
                }

                if (auto *bindless_mgr = gc->GetBindlessTextureManager();
                    bindless_mgr && bindless_mgr->IsValid())
                {
                    bindless_mgr->BindToCmd(*cmd_buf,
                                            layout,
                                            static_cast<uint32_t>(graph::DescriptorSetType::Bindless));
                }
            }
        }

        // 重置渲染状态缓存（每批次 ICB 命令从 0 开始）
        last_data_buffer = nullptr;
        indirect_draw_count = 0;
        indirect_draw_command_offset = 0;
        first_indirect_draw_index = -1;

        // SSBO 顶点输入判定（schema 含顶点数据 SSBO 需求）——MeshShader 方向：
        // 顶点 buffer 按对象绑定（VDM 共享 buffer 同值；独立 VAB 每对象正确）
        ssbo_vertex_input = false;
        last_ssbo_pos = VK_NULL_HANDLE;
        last_ssbo_uv  = VK_NULL_HANDLE;
        last_ssbo_ntb = VK_NULL_HANDLE;
        last_ssbo_index = VK_NULL_HANDLE;
        {
            const auto &schema = material->GetShaderResourceSchema();
            for (const auto &res : schema.resources)
            {
                if (res.semantic == graph::mtl::DescriptorSemantic::VertexPosition)
                {
                    ssbo_vertex_input = true;
                    break;
                }
            }
        }

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

        // 遍历绘制批次
        DrawBatch* batch = const_cast<DrawBatch*>(batches.data());

        for (uint32_t i = 0; i < batch_count; i++)
        {
            Draw(batch, transform_buffer, icb_draw, owner_batch);
            ++batch;
        }

        // 提交剩余的间接绘制命令
        if (indirect_draw_count)
        {
            ProcIndirectRender(icb_draw);
        }
    }
}//namespace hgl::ecs
