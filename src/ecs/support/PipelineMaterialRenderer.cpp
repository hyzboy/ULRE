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
#include<hgl/graph/ShaderBufferSources.h>

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
        for (auto *mp : per_object_mp_pool)
            delete mp;
        per_object_mp_pool.clear();
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

    void PipelineMaterialRenderer::BindMeshDrawParamsView(const DrawBatch* batch)
    {
        // 直接绘制路径：参数表按本 draw 行 offset 视图重绑（gl_DrawID=0 → rows[0]
        // 恰为本行）。run 起点的 switch 绑定已覆盖单 draw run——offset 缓存跳过。
        if (!cur_owner_batch || !cur_owner_batch->mesh_draw_params_buffer || !batch)
            return;

        auto *mp = batch->per_object_mp;
        if (!mp)
            return;

        const VkDeviceSize offset =
            static_cast<VkDeviceSize>(batch->params_row)
            * sizeof(graph::mtl::MeshDrawParams);

        if (offset == last_mesh_params_offset)
            return;

        mp->BindSSBO("mesh_draw_params",
                     cur_owner_batch->mesh_draw_params_buffer->GetGPUBuffer()->GetVkDeviceBuffer(),
                     offset,
                     VK_WHOLE_SIZE);
        mp->Update();
        const VkDescriptorSet ds = mp->GetVkDescriptorSet();
        cmd_buf->BindDescriptorSets(material->GetPipelineLayout(),
                                    static_cast<uint32_t>(graph::DescriptorSetType::PerObject),
                                    &ds, 1, nullptr, 0);
        last_mesh_params_offset = offset;
    }

    bool PipelineMaterialRenderer::Draw( DrawBatch* batch,
                                            TransformAssignmentBuffer* transform_buffer,
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
                ProcIndirectRender();
            }

            // 更新缓冲状态
            last_data_buffer = batch->geom_data_buffer;

            // SSBO 顶点输入：把当前 DrawBatch 的顶点/索引 buffer 绑到 PerObject set
            //（switch 触发即对目标 set 全量重绑，不做逐语义"buffer 未变跳过"——
            //  目标 set 是 per-DrawBatch 独立的，按渲染器全局缓存跳过在部分语义
            //  共享 buffer 时会漏绑；切换很少发生（VDM 共享 buffer 整批一次），
            //  全量绑定开销可忽略）
            if (ssbo_vertex_input)
            {
                auto *mp = batch->per_object_mp;

                if (mp)
                {
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
                            mp->BindSSBO("VertexPosition", buf, 0, VK_WHOLE_SIZE);
                            break;
                        case graph::VertexSemantic::TexCoord:
                            mp->BindSSBO("VertexUV", buf, 0, VK_WHOLE_SIZE);
                            break;
                        case graph::VertexSemantic::Normal:
                            mp->BindSSBO("VertexNTB", buf, 0, VK_WHOLE_SIZE);
                            break;
                        case graph::VertexSemantic::Color:
                            mp->BindSSBO("VertexColor", buf, 0, VK_WHOLE_SIZE);
                            break;
                        case graph::VertexSemantic::Luminance:
                            mp->BindSSBO("VertexLuminance", buf, 0, VK_WHOLE_SIZE);
                            break;
                        case graph::VertexSemantic::TransformID:
                            mp->BindSSBO("VertexTransformID", buf, 0, VK_WHOLE_SIZE);
                            break;
                        case graph::VertexSemantic::Size:
                            mp->BindSSBO("VertexSize", buf, 0, VK_WHOLE_SIZE);
                            break;
                        default: break;
                        }
                    }

                    // geometry 直取（渲染路径的 geom_data_buffer 无 VAB 数据
                    // （vab_count=0——PrimitiveComponent runtime buffer）时的补绑）
                    if (batch->geometry && geom_buffer->vab_count == 0)
                    {
                        static const struct
                        {
                            graph::VertexSemantic semantic;
                            const char *name;
                        } geometry_vab_bindings[] =
                        {
                            {graph::VertexSemantic::Position,   "VertexPosition"},
                            {graph::VertexSemantic::TexCoord,   "VertexUV"},
                            {graph::VertexSemantic::Normal,     "VertexNTB"},
                            {graph::VertexSemantic::Color,      "VertexColor"},
                            {graph::VertexSemantic::Luminance,  "VertexLuminance"},
                            {graph::VertexSemantic::Size,       "VertexSize"},
                        };

                        for (const auto &binding : geometry_vab_bindings)
                        {
                            if (auto *vab = batch->geometry->GetVAB(binding.semantic))
                                mp->BindSSBO(binding.name, vab->GetVkBuffer(), 0, VK_WHOLE_SIZE);
                        }
                    }

                    // 顶点索引 SSBO（非索引绘制——索引数据统一 SSBO）：
                    // IBO buffer 绑 VertexIndex 槽（VDM 共享/独立 IBO 均正确——段偏移走 push constant）
                    if (geom_buffer->ibo)
                        mp->BindSSBO("VertexIndex", geom_buffer->ibo->GetVkBuffer(), 0, VK_WHOLE_SIZE);

                    // l2w / index rows 补绑（独立 VAB 场景的 program 实例可能没有
                    // RDBS 预绑——Draw 侧统一补到本 draw 的 PerObject MP）
                    if (transform_buffer)
                        transform_buffer->BindTransform(mp);

                    if (owner_batch)
                    {
                        if (owner_batch->l2w_index_rows_buffer)
                            mp->BindSSBO("l2w_index_rows",
                                         owner_batch->l2w_index_rows_buffer->GetGPUBuffer());

                        if (owner_batch->material_data_index_rows_buffer)
                            mp->BindSSBO("mtl_data_index_rows",
                                         owner_batch->material_data_index_rows_buffer->GetGPUBuffer());
                    }

                    // IndirectMeshDraw：mesh per-draw 参数表——按 run 首行 offset 绑定
                    //（switch 触发即 run 起点；rows[gl_DrawID] 在 run 内与命令序 1:1 对齐）
                    if (material_is_mesh
                     && owner_batch
                     && owner_batch->mesh_draw_params_buffer)
                    {
                        const VkDeviceSize params_offset =
                            static_cast<VkDeviceSize>(batch->params_row)
                            * sizeof(graph::mtl::MeshDrawParams);
                        mp->BindSSBO(
                            "mesh_draw_params",
                            owner_batch->mesh_draw_params_buffer->GetGPUBuffer()->GetVkDeviceBuffer(),
                            params_offset,
                            VK_WHOLE_SIZE);
                        last_mesh_params_offset = params_offset;
                    }

                    // 一次 Update + 绑定（此前分三段各自 Update/绑定一次）
                    mp->Update();
                    const VkDescriptorSet ds = mp->GetVkDescriptorSet();
                    cmd_buf->BindDescriptorSets(material->GetPipelineLayout(),
                                                static_cast<uint32_t>(graph::DescriptorSetType::PerObject),
                                                &ds, 1, nullptr, 0);
                }
                else if (transform_buffer)
                {
                    // per-draw MP 创建失败兜底：l2w 绑材质默认 PerObject（与旧路径一致）
                    transform_buffer->BindTransform(material->GetMP(graph::DescriptorSetType::PerObject));
                }
            }
        }

        // IndirectMeshDraw：mesh 材质 per-draw 参数走 mesh_draw_params 参数表 SSBO
        //（BuildBatches 与命令同序写行），不再推 push constant。
        // 非 mesh（手写管线遗留）：12B push + Draw 原路径。
        if (ssbo_vertex_input)
        {
            if (material_is_mesh)
            {
                // 实例化：Y 轴 = 实例轴（gl_WorkGroupID.y = 实例内序号，
                // + first_instance 后与 l2w_index_rows 行号对齐）
                // total_vertices：索引几何 = index_count（每索引 1 顶点查表），非索引 = vertex_count
                const bool is_lines = material->GetPrimitiveType() == hgl::graph::PrimitiveType::Lines;
                const uint32_t total_vertices = batch->geom_draw_range->index_count > 0
                    ? static_cast<uint32_t>(batch->geom_draw_range->index_count)
                    : static_cast<uint32_t>(batch->geom_draw_range->vertex_count);
                const uint32_t group_count = CalcMeshGroupCount(is_lines, total_vertices);
                const uint32_t instance_count = batch->instance_count > 1
                    ? static_cast<uint32_t>(batch->instance_count)
                    : 1u;

                // 间接合批：VDM 共享 buffer + MDI 支持 → 累积命令（buffer 切换/批末
                // flush 一条 vkCmdDrawMeshTasksIndirectEXT）。
                // 不支持 MDI 时走直接路径——逐条 fallback 的 DrawID 恒 0，无法区分命令
                const bool use_indirect = batch->geom_data_buffer->vdm
                                       && owner_batch
                                       && owner_batch->icb_mesh_tasks
                                       && owner_batch->device
                                       && owner_batch->device->GetPhyDevice()->SupportMDI();

                if (use_indirect)
                {
                    // 命令偏移取本批 ICB 命令序号累计（与 BuildBatches 写入序一致；
                    // 不能用 first_instance——vdm/非 vdm 混排时二者脱节）
                    if (indirect_draw_count == 0)
                    {
                        first_indirect_draw_index =
                            static_cast<int32_t>(indirect_draw_command_offset);
                    }

                    ++indirect_draw_count;
                }
                else
                {
                    // 直接路径（私有 VBO / 无 MDI）：参数表 offset 视图重绑到本 draw 行
                    //（gl_DrawID=0 → rows[0] 恰为本行）后直接 dispatch
                    BindMeshDrawParamsView(batch);
                    cmd_buf->DrawMeshTasks(group_count, instance_count);
                }

                return true;
            }
        }

        // mesh 为唯一顶点路径（VS 已彻底删除）——SSBO 顶点输入材质必走上方 mesh
        // 分支；执行到此说明材质 schema 异常（无顶点 SSBO 且非 mesh），无绘制路径
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
        last_mesh_params_offset = UINT64_MAX;
        cur_owner_batch = owner_batch;

        // mesh stage 判定（批级一次——switch 块参数表绑定与间接 flush 分派共用）
        material_is_mesh = false;
        for (const auto &stage : material->GetStageList())
        {
            if (stage.stage == VK_SHADER_STAGE_MESH_BIT_EXT)
            {
                material_is_mesh = true;
                break;
            }
        }

        // SSBO 顶点输入判定（schema 含顶点数据 SSBO 需求）——MeshShader 方向：
        // 顶点 buffer 按对象绑定（VDM 共享 buffer 同值；独立 VAB 每对象正确）
        ssbo_vertex_input = false;
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

        // per-draw 独立 PerObject MP 池（descriptor set 是状态非快照——多对象独立
        // buffer 时共享单 set 被 per-draw 顺序更新，提交时刻所有 draw 读最后一次
        // 更新的内容；每 draw 独立 set 各自更新+绑定）
        // VDM 共享 buffer：整批 GeometryDataBuffer 内容相同——单 set 即可（仅首
        // draw 触发绑定，后续 draw 无切换直接沿用；避免按绘制数分配空置 set）
        const bool need_per_draw_mp = ssbo_vertex_input;
        bool single_per_object_set = true;

        if (need_per_draw_mp && batch_count > 1)
        {
            const auto *first_db = batch[0].geom_data_buffer;

            if (!first_db)
                single_per_object_set = false;

            for (uint32_t i = 1; i < batch_count; i++)
            {
                const auto *db = batch[i].geom_data_buffer;
                if (!db || *db != *first_db)
                {
                    single_per_object_set = false;
                    break;
                }
            }
        }

        const uint32_t mp_count = !need_per_draw_mp ? 0u
                              : single_per_object_set ? 1u
                              : batch_count;

        if (per_object_mp_pool.size() < mp_count)
            per_object_mp_pool.resize(mp_count, nullptr);

        for (uint32_t i = 0; i < batch_count; i++)
        {
            if (need_per_draw_mp)
            {
                const uint32_t pool_index = single_per_object_set ? 0u : i;

                if (!per_object_mp_pool[pool_index])
                {
                    auto *base_mp = material->GetMP(graph::DescriptorSetType::PerObject);

                    if (base_mp && owner_batch && owner_batch->device)
                        per_object_mp_pool[pool_index] = owner_batch->device->CreateMP(base_mp->GetDescManager(),
                                                                              material->GetPipelineLayoutData(),
                                                                              graph::DescriptorSetType::PerObject);
                }

                batch->per_object_mp = per_object_mp_pool[pool_index];
            }

            Draw(batch, transform_buffer, owner_batch);
            ++batch;
        }

        // 提交剩余的间接绘制命令
        if (indirect_draw_count)
        {
            ProcIndirectRender();
        }
    }
}//namespace hgl::ecs
