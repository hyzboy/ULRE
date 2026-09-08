/**
 * PipelineMaterialRenderer.cpp - ECS Pipeline材质渲染器实现
 *
 * 参照 PipelineMaterialRenderer 实现，但使用 ECS 版本的 Assignment Buffers
 */

#include<hgl/ecs/support/PipelineMaterialRenderer.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/log/Log.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/vk/VKGlobalSceneUBOSet.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKIndirectCommandBuffer.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/graph/RootAddressPush.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/mtl/DescriptorResourceCatalog.h>

namespace hgl::ecs
{
    PipelineMaterialRenderer::PipelineMaterialRenderer(graph::ShaderProgram* m, graph::Pipeline* p)
        : material(m)
        , pipeline(p)
        , cmd_buf(nullptr)
        , first_indirect_draw_index(-1)
        , indirect_draw_count(0)
    {
    }

    void PipelineMaterialRenderer::ProcIndirectRender()
    {
        // 提交累积的 mesh 间接绘制命令：一条 multi-draw（每命令 {X=组数, Y=实例数, Z=1}）
        //——per-draw 段偏移经 mesh_draw_params 参数表查表（rows[gl_DrawID]）
        if (cur_owner_batch && cur_owner_batch->icb_mesh_tasks)
        {
            static bool s_logged_once = false;
            if (!s_logged_once)
            {
                s_logged_once = true;
                GLogInfo(u8"[IndirectMeshDraw] mesh indirect flush engaged: first=%d count=%u",
                         first_indirect_draw_index, indirect_draw_count);
            }
            cmd_buf->DrawMeshTasksIndirect(
                cur_owner_batch->icb_mesh_tasks->GetVkBuffer(),
                static_cast<VkDeviceSize>(first_indirect_draw_index)
                    * sizeof(VkDrawMeshTasksIndirectCommandEXT),
                indirect_draw_count);
        }

        // 重置间接绘制状态（命令序号累计到本批次已提交段）
        first_indirect_draw_index = -1;
        indirect_draw_command_offset += indirect_draw_count;
        indirect_draw_count = 0;
    }

    bool PipelineMaterialRenderer::Draw( DrawBatch* batch,
                                            TransformAssignmentBuffer* transform_buffer,
                                            const MaterialBatch *owner_batch)
    {
        (void)transform_buffer;
        (void)owner_batch;

        // mesh 为唯一顶点路径（VS 已彻底删除）：顶点数据与 per-draw 参数全部经
        // MeshDrawParams 行内 BDA 寻址——无 buffer 切换、无 per-draw descriptor
        //（BDA 使能前 need_buffer_switch 会在 buffer 变化时 flush + 重绑 PerObject set，
        //  该机制已随 7 表全 BDA 化退场）。所有 DrawBatch（含私有 VBO）累积为同一条
        //  vkCmdDrawMeshTasksIndirectEXT multi-draw：命令序 = DrawBatch 序 = 参数行序
        // （gl_DrawID 1:1），批末由 Render 统一 flush。
        if (material_is_mesh)
        {
            if (indirect_draw_count == 0)
            {
                first_indirect_draw_index =
                    static_cast<int32_t>(indirect_draw_command_offset);
            }

            ++indirect_draw_count;
        }

        return true;
    }

    void PipelineMaterialRenderer::Render(graph::RenderCmdBuffer* rcb,
                                              const DrawBatchArray& batches,
                                              uint32_t batch_count,
                                              TransformAssignmentBuffer* transform_buffer,
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

        // EDS 1/2/3：pipeline 只保留 shader 部分——材质渲染状态渲染侧动态应用
        if (pipeline)
            cmd_buf->ApplyPipelineState(pipeline->GetConfig());

        // Set 0（Scene UBO）/ Set 3（Bindless 纹理）按材质自身 layout 绑定。
        // VVL 的 set 兼容 ID 取 layout 在 set 0..N 的全部 DSL 前缀，绑定 layout 必须与
        // draw 时管线 layout（= 材质 pipeline layout）一致。旧方案由
        // RenderSceneUBOSystem 用"第一个活跃材质"的 layout 统一绑定，
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

        // 重置间接命令状态（每批次从 0 开始；本批所有 DrawBatch 累积后一次 flush）
        indirect_draw_count = 0;
        indirect_draw_command_offset = 0;
        first_indirect_draw_index = -1;
        cur_owner_batch = owner_batch;

        // mesh stage 判定（批级一次——push constant 与间接 flush 分派共用）
        material_is_mesh = false;
        for (const auto &stage : material->GetStageList())
        {
            if (stage.stage == VK_SHADER_STAGE_MESH_BIT_EXT)
            {
                material_is_mesh = true;
                break;
            }
        }

        // RootAddresses push constant：每 MaterialBatch 渲染前一次（draw 之前）。A3-1：
        // mesh shader 经 pc_root.addr_mesh_draw_params buffer_reference 解引用参数表行
        //（rows[gl_DrawID]）；L2W/L2WIndex/mtl_data_addrs 地址一并下发（A3-2/3 起 shader
        // 消费——行表 buffer 已带 SHADER_DEVICE_ADDRESS usage，地址可取即填）。
        if (material_is_mesh && owner_batch && owner_batch->device)
        {
            graph::IGPUBuffer *l2w_gpu = nullptr;
            if (transform_buffer)
            {
                auto *l2w_buf = transform_buffer->GetTransformDataBuffer();
                if (l2w_buf)
                    l2w_gpu = l2w_buf->GetGPUBuffer();
            }

            graph::PushRootAddresses(
                cmd_buf,
                owner_batch->device,
                material->GetPipelineLayout(),
                owner_batch->mesh_draw_params_buffer
                    ? owner_batch->mesh_draw_params_buffer->GetGPUBuffer() : nullptr,
                l2w_gpu,
                owner_batch->l2w_index_buffer
                    ? owner_batch->l2w_index_buffer->GetGPUBuffer() : nullptr,
                owner_batch->material_data_index_rows_buffer
                    ? owner_batch->material_data_index_rows_buffer->GetGPUBuffer() : nullptr);
        }

        // 批次级描述符覆盖（batch_descriptor_mp）与材质级绑定（BindDescriptorSets(material)）
        // 已随 desc_manager/MP 机制整体退役删除（2026-09-08）：Scene/Bindless 由设备级
        // 全局绑定（VKGlobalSceneUBOSet / VKBindlessTextureManager），材质侧不再绑任何集。

        // 遍历绘制批次：全部累积命令（BDA 后无 per-draw descriptor/set——BDA 化前
        // 的 per-draw 独立 PerObject MP 池机制已随 7 表全 BDA 退场）
        DrawBatch* batch = const_cast<DrawBatch*>(batches.data());

        for (uint32_t i = 0; i < batch_count; i++)
        {
            Draw(batch, transform_buffer, owner_batch);
            ++batch;
        }

        // 批末统一 flush 一条 vkCmdDrawMeshTasksIndirectEXT（multi-draw）
        if (indirect_draw_count)
        {
            ProcIndirectRender();
        }
    }
}//namespace hgl::ecs
