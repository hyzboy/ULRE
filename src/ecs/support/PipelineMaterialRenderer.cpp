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
#include<hgl/graph/ubo/GlobalAddresses.h>
#include<hgl/log/Log.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/buffer/IndexBuffer.h>
#include<hgl/vk/buffer/VertexAttribBuffer.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/buffer/IndirectCommandBuffer.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/graph/RootAddressPush.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/vk/VKBufferReadback.h>

namespace hgl::ecs
{
    PipelineMaterialRenderer::PipelineMaterialRenderer(graph::ShaderProgram* m, graph::Pipeline* p)
        : material(m)
        , pipeline(p)
        , cmd_buf(nullptr)
    {
    }

    void PipelineMaterialRenderer::ProcIndirectRender()
    {
        // 提交累积的 mesh 间接绘制命令：一条 multi-draw（每命令 {X=组数, Y=实例数, Z=1}）
        //——per-draw 段偏移经 mesh_draw_params 参数表查表（rows[gl_DrawID]）
        if (cur_owner_batch && cur_owner_batch->icb_mesh_tasks)
        {
            // T3i（③ 多批）：GPU 托管批的段偏移/段长 = **本批切片**（CPU 侧不再写 ICB
            // 与行表 ⇒ `first_indirect_draw_index`/`indirect_draw_count` 只是 items 计数，
            // 不是段位置）。段偏移 = `row_base × 12B`（ICB 是共享竞技场）；
            // maxDrawCount = 切片容量，实际条数由本批 GPU 计数给出。
            // T4c3c：切片**按当前 pass** 取（批对象跨 pass 共享，两 pass 各有自己的块/扇区）。
            const MaterialBatch::GpuSlice &slice = cur_owner_batch->GetGpuSlice(cur_pass_index);
            const int32_t  first     = static_cast<int32_t>(slice.row_base);
            const uint32_t max_count = slice.capacity;

            // 诊断接线（`ECS_TAKEOVER_TRACE=1`）：绘制侧读的槽/切片 —— 与派发侧对照
            static const bool s_flush_trace = (getenv("ECS_TAKEOVER_TRACE") != nullptr);
            if (s_flush_trace)
                // 槽 = 偏移 / 2056（每槽 2 pass × 257 行 × 4 B）；此处只打原始偏移，离线换算
                GLogInfo("[T3i][TRACE] flush first=%d max=%u gpu_driven=%d 计数偏移=%u pass=%u",
                         first, max_count, cur_owner_batch->gpu_driven_override ? 1 : 0,
                         slice.count_buffer_offset, cur_pass_index);

            static bool s_logged_once = false;
            if (!s_logged_once)
            {
                s_logged_once = true;
                GLogInfo(u8"[IndirectMeshDraw] mesh indirect flush engaged: first=%d count=%u"
                         u8"（gpu_driven=%d row_base=%u capacity=%u）",
                         first, max_count,
                         cur_owner_batch->gpu_driven_override ? 1 : 0,
                         slice.row_base, slice.capacity);
            }

            if (cur_owner_batch->icb_count_buffer)
            {
                // SSBO DUMP 配套（CULL_SSBO_DUMP=1 才付出同步回读；记录绘制侧真正消费的计数值）
                static const bool s_count_dump = (getenv("CULL_SSBO_DUMP") != nullptr);
                if (s_count_dump)
                {
                    uint32_t current_count_val = UINT32_MAX;
                    if (graph::ReadbackBuffer(cur_owner_batch->icb_count_buffer, slice.count_buffer_offset, sizeof(uint32_t), &current_count_val))
                    {
                        GLogInfo(u8"[IndirectMeshDraw][COUNT_DUMP] first=%d max=%u gpu_driven=%d 计数偏移=%u (槽=%u pass=%u) => COUNT_IN_BUFFER=%u",
                                 first, max_count, cur_owner_batch->gpu_driven_override ? 1 : 0,
                                 slice.count_buffer_offset, slice.count_buffer_offset / 2056, cur_pass_index, current_count_val);
                    }
                }

                cmd_buf->DrawMeshTasksIndirectCount(
                    cur_owner_batch->icb_mesh_tasks->GetVkBuffer(),
                    static_cast<VkDeviceSize>(first) * sizeof(VkDrawMeshTasksIndirectCommandEXT),
                    cur_owner_batch->icb_count_buffer->GetBuffer(),
                    slice.count_buffer_offset,
                    max_count);
            }
            else
            {
                cmd_buf->DrawMeshTasksIndirect(
                    cur_owner_batch->icb_mesh_tasks->GetVkBuffer(),
                    static_cast<VkDeviceSize>(first) * sizeof(VkDrawMeshTasksIndirectCommandEXT),
                    max_count);
            }
        }

        // 重置间接绘制状态（命令序号累计到本批次已提交段）
    }

    void PipelineMaterialRenderer::Render(graph::RenderCmdBuffer* rcb,
                                          TransformAssignmentBuffer* transform_buffer,
                                          const MaterialBatch *owner_batch,
                                          graph::RenderContext *render_context,
                                          uint32_t camera_row,
                                          uint32_t pass_index)
    {
        // 前置条件检查
        if (!rcb)
        {
            GLogError("[PipelineMaterialRenderer::Render] No render command buffer");
            return;
        }

        cmd_buf = rcb;

        // 绑定管线
        cmd_buf->BindPipeline(pipeline);

        // EDS 1/2/3：pipeline 只保留 shader 部分——材质渲染状态渲染侧动态应用
        if (pipeline)
            cmd_buf->ApplyPipelineState(pipeline->GetConfig());

        // 全局集（Scene/Bindless）绑定：共享 pipeline layout 下每 cmd 首绑一次
        //（GraphicsContext::BindGlobalDescriptorSets 守卫），本批重复调用自动跳过。
        if (render_context)
        {
            if (auto *gc = render_context->GetGraphicsContext())
                gc->BindGlobalDescriptorSets(cmd_buf, material->GetPipelineLayout());
        }

        // 重置间接命令状态（每批次从 0 开始；本批所有 DrawBatch 累积后一次 flush）
        cur_owner_batch = owner_batch;
        cur_pass_index = pass_index;   // T4c3c：切片/计数按本 pass 取

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
        // mesh shader 经 pc_root.addr_batch_mesh_draw_params buffer_reference 解引用参数表行
        //（rows[gl_DrawID]）；L2W/L2WIndex/mtl_data_addrs 地址一并下发（A3-2/3 起 shader
        // 消费——行表 buffer 已带 SHADER_DEVICE_ADDRESS usage，地址可取即填）。
        if (material_is_mesh && owner_batch && owner_batch->device)
        {
            graph::IGPUBuffer *l2w_gpu = nullptr;
            if (owner_batch->l2w_buffer)
            {
                l2w_gpu = owner_batch->l2w_buffer->GetGPUBuffer();
            }
            else if (transform_buffer)
            {
                auto *l2w_buf = transform_buffer->GetTransformDataBuffer();
                if (l2w_buf)
                    l2w_gpu = l2w_buf->GetGPUBuffer();
            }

            // 全局地址表基址 = 本次 push 的根入口（其余全局地址都在表内，不再逐个 push）
            graph::GraphicsContext *addr_gc =
                render_context ? render_context->GetGraphicsContext() : nullptr;
            // 表按帧槽多份：本批次属于哪一帧槽由相机行号决定（行号 = 槽号 * 槽总数 + 帧槽）。
            const uint64_t addr_global_addresses =
                addr_gc ? addr_gc->GetGlobalAddressesAddress(
                              graph::GlobalAddressesSlotFromCameraRow(camera_row)) : 0;

            graph::PushRootAddresses(
                cmd_buf,
                owner_batch->device,
                material->GetPipelineLayout(),
                addr_global_addresses,
                owner_batch->world_addresses_address,
                owner_batch->mesh_draw_params_buffer
                    ? owner_batch->mesh_draw_params_buffer->GetGPUBuffer() : nullptr,
                l2w_gpu,
                owner_batch->l2w_index_buffer
                    ? owner_batch->l2w_index_buffer->GetGPUBuffer() : nullptr,
                owner_batch->material_data_index_rows_buffer
                    ? owner_batch->material_data_index_rows_buffer->GetGPUBuffer() : nullptr,
                owner_batch->texture_reference_base_addr,
                nullptr, nullptr, nullptr,
                camera_row,
                // T3i（③ 多批）：本批切片起点 ⇒ 三表 push 地址各加 `row_base × 行字节`
                //（cmds 8B / l2w_index 4B / mtl 16B；row_base % 4 == 0 ⇒ 三者皆 16B 对齐，
                //  与 shader 侧 buffer_reference_align 声明相符）。非托管批 row_base=0。
                // T4c3c：切片按当前 pass 取。
                VkDeviceSize(owner_batch->GetGpuSlice(cur_pass_index).row_base) * 8u,
                VkDeviceSize(owner_batch->GetGpuSlice(cur_pass_index).row_base) * 4u,
                VkDeviceSize(owner_batch->GetGpuSlice(cur_pass_index).row_base) * 16u);
        }

        // 批次级描述符覆盖（batch_descriptor_mp）与材质级绑定（BindDescriptorSets(material)）
        // 已随 desc_manager/MP 机制整体退役删除（2026-09-08）：Scene/Bindless 由设备级
        // 全局绑定（VKBindlessTextureManager，唯一集合），材质侧不再绑任何集。

        // 遍历绘制批次：全部累积命令（BDA 后无 per-draw descriptor/set——BDA 化前
        // 的 per-draw 独立 PerObject MP 池机制已随 7 表全 BDA 退场）
        //
        // T4c4-3：CPU 绘制批已删 ⇒ 这里只剩接管批的 flush（间接命令与条数都由 GPU gather 写）。
        if (owner_batch && owner_batch->gpu_driven_override)
        {
            ProcIndirectRender();
        }
    }
}//namespace hgl::ecs
