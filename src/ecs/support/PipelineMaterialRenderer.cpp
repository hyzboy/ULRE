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
        for (auto *mp : per_object_mp_pool)
            delete mp;
        per_object_mp_pool.clear();
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

        // per-draw 段偏移 push constant（每 DrawBatch 必推——VDM 共享 buffer 各模型
        // 段偏移不同；间接累积提交无法 per-draw——SSBO 材质直接绘制）
        if (ssbo_vertex_input)
        {
            // mesh shader 材质（ShaderGen 全量 mesh 化后唯一路径）：20B push
            // （index_base/vertex_base/is_indexed/total_vertices/viewport_height）
            // + DrawMeshTasks（每线程 1 顶点，threadgroup=64）。
            // 非 mesh（示例手写管线 VS 材质）：12B push + Draw 原路径。
            bool is_mesh = false;
            for (const auto &stage : material->GetStageList())
            {
                if (stage.stage == VK_SHADER_STAGE_MESH_BIT_EXT)
                {
                    is_mesh = true;
                    break;
                }
            }

            if (is_mesh)
            {
                // 实例化：DrawMeshTasks(gx, instance_count)——gl_WorkGroupID.y = 实例索引
                //（mesh shader 的 gl_InstanceIndex 宏映射；顶点读取是实例内序号，
                //  VDM 共享顶点/独立 VAB 均正确——每实例重复同一批顶点）
                struct MeshPC
                {
                    uint32_t index_base;
                    uint32_t vertex_base;
                    uint32_t is_indexed;
                    uint32_t total_vertices;
                    float    viewport_height;
                    uint32_t first_instance;
                } pc{};

                pc.index_base      = static_cast<uint32_t>(batch->geom_draw_range->first_index);
                pc.vertex_base     = static_cast<uint32_t>(batch->geom_draw_range->vertex_offset);
                pc.is_indexed      = batch->geom_draw_range->index_count > 0 ? 1u : 0u;
                // total_vertices：索引几何=index_count（每索引 1 顶点查表），非索引=vertex_count（直通）
                // ——与 VS 的 vertexCount 双语义一致（skill §10）；实例化时是每实例顶点数
                pc.total_vertices  = batch->geom_draw_range->index_count > 0
                                   ? static_cast<uint32_t>(batch->geom_draw_range->index_count)
                                   : static_cast<uint32_t>(batch->geom_draw_range->vertex_count);
                pc.viewport_height = cmd_buf->GetViewport().height;
                // first_instance：l2w_index_rows 按整批 item 序号写，mesh 实例索引 =
                // first_instance + gl_WorkGroupID.y（与 VS 的 gl_InstanceIndex 语义一致）
                pc.first_instance  = batch->first_instance;
                cmd_buf->PushConstants(material->GetPipelineLayout(), &pc, sizeof(pc));

                // Mesh shader 绘制：threadgroup 大小按图元类型——Lines（LineQuad）每线程
                // 1 线段 = 2 顶点 → 线段数 = total_vertices/2，组大小 64；
                // 其它（VertexPassthrough）每线程 1 顶点，组大小 96（3 的倍数——组内
                // 三角形永不跨组，避免 64 边界丢三角形）
                const bool is_lines = material->GetPrimitiveType() == hgl::graph::PrimitiveType::Lines;
                const uint32_t process_count = is_lines ? (pc.total_vertices >> 1u) : pc.total_vertices;
                const uint32_t group_size = is_lines ? 64u : 96u;
                const uint32_t group_count = (process_count + group_size - 1u) / group_size;
                const uint32_t instance_count = batch->instance_count > 1
                                              ? static_cast<uint32_t>(batch->instance_count)
                                              : 1u;
                cmd_buf->DrawMeshTasks(group_count, instance_count);
                return true;
            }

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
