#include<hgl/ecs/support/PrimitiveBatchPipeline.h>
#include<hgl/ecs/support/RenderStrategyParity.h>
#include<hgl/ecs/support/BoundingVolumeCull.h>
#include<hgl/ecs/support/PerfTimer.h>
#include<source_location>
#include<cmath>
#include<cstdlib>
#include<cstdio>
#include<cstring>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/BoundingBoxAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/ShadowProxy.h>
#include<hgl/ecs/support/PrimitiveState.h>
#include<hgl/ecs/support/GPUCullSystem.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/graph/module/SSBOBufferRegistry.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/vk/VertexDataManager.h>

#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/util/hash/FNV1a.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/vk/buffer/IndirectCommandBuffer.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderPass.h>
#include<hgl/common/DebugChannel.h>
#include<hgl/log/Log.h>
#include<hgl/time/Time.h>
#include<algorithm>
#include<cstdint>
#include<string>
#include<unordered_map>

namespace hgl::ecs
{
    namespace
    {
        // 材质来源判据已收敛为实体级自由函数 HasAnyMaterialSource(Entity*)（support/PrimitiveState.h）。

        // 材质是否含 mesh stage（ShaderGen 材质恒是——mesh 为唯一顶点路径）
        bool ProgramHasMeshStage(const graph::ShaderProgram *program)
        {
            if (!program)
                return false;

            for (const auto &stage : program->GetStageList())
            {
                if (stage.stage == VK_SHADER_STAGE_MESH_BIT_EXT)
                    return true;
            }

            return false;
        }

        bool BatchRequiresIndirectCommands(const MaterialBatch &batch)
        {
            // BDA 后顶点数据经行内地址寻址——所有 DrawBatch（含私有 VBO）都进
            // 同一次 vkCmdDrawMeshTasksIndirectEXT multi-draw，命令序 = 行序 1:1。
            // 凡含几何项的 batch 都需要 ICB（mesh 为唯一顶点路径，恒走间接）。
            for (auto *item : batch.items)
            {
                // B3-iii：判据从"有每实体 CPU 几何副本"改为"行键有几何"（等价：副本只在有几何时存在）
                if (item && item->draw_key.HasGeometry())
                    return true;
            }

            return false;
        }


        // A6-2b/b3：BDA 后无 per-material SSBO 绑定差异（材质数据/顶点流全走行内
        // 地址与 pc_root），合批 key 只按 shader+pipeline 区分——同 shader 即同批。
    }

    bool PrimitiveBatchPipeline::PrepareFrame(ECSContext* ctx)
    {
        if (!ctx)
            return false;

        // §2 #11（2026-10-11）：**每帧只准备一次**的真守卫。
        // `PrimitiveRenderPipeline::RunCull/RunSort/RunBuild` 各自调用本函数 ⇒ 原实现每帧最多
        // 做三遍同样的帧前准备（其中 Sort 那遍没有任何下游动作）。守卫 = 帧序列号 + 结果缓存：
        // 同一帧的后续调用直接返回首次结果（含失败），下一帧才开始重新准备。
        //
        // ⚠ 键必须是 `RenderFrameCache::frame_serial`（每次 BeginFrame 自增），**不能**是
        // `ECSContext::GetFrameIndex()`：后者是驱动相机的**帧槽号**，离屏 pass 恒为该 RT 的
        // 固定槽 ⇒ 离屏世界永不推进 ⇒ 首帧准备失败（首帧 renderItems 必为空）被永久缓存，
        // `RunBatching()` 再也不被调用、`materialBatches` 恒空 ⇒ 离屏世界永不绘制
        // （RenderToTexture / RenderToTextureColorDepth 两例的最终根因）。
        auto& cache = ctx->GetRenderFrameCache();
        const uint32_t frame_serial = cache.frame_serial;

        if (prepared_frame_ == frame_serial)
            return prepared_frame_ok_;

        ++prepare_frame_count_;
        prepared_frame_      = frame_serial;
        prepared_frame_ok_   = PrepareFrameInternal(ctx);

        return prepared_frame_ok_;
    }

    bool PrimitiveBatchPipeline::PrepareFrameInternal(ECSContext* ctx)
    {
        if (!ctx)
            return false;

        world = ctx;
        auto& cache = world->GetRenderFrameCache();
        if (cache.renderItems.empty())
            return false;

        camera_info = cache.cameraInfo;
        if (!camera_info)
            return false;

        if (!device)
            device = world->GetGPUDevice();

        return true;
    }

    // ULRE_DEBUG.toml 的 channels.perf = true 计时代（三处独立累计，函数内 static ⇒ 零头文件改动）：
    //   cull  = PerformFrustumCulling 的 CPU 时间（形态 B 接管帧 = 0）
    //   batch = RunBatching 的 CPU 时间（形态 B guard+接管帧仍要建批，含差异）
    // 计时器/RAII 段本体见 `support/PerfTimer.h`（与 `GPUCullSystem` 的派发计时共用一份实现）。
    void PrimitiveBatchPipeline::RunCulling()
    {
        static PerfTimer s_cull_timer("cpu_cull");

        // T3f-2（形态 B，判据 2）：上一帧 guard 判定接管 ⇒ 本帧**跳过 CPU 剔除**。
        // 时序口径：guard 在 `FinalizeBatches()`（RenderBatch，批建成*之后*）评估，
        // 批成员资格在 `BuildMaterialBatches` 按 isVisible 过滤 ⇒ "跳不跳"只能取
        // **上一帧**的结果；稳态下批 = 全项（新 item 默认 isVisible=true），
        // 幸存数 ≤ 批容量 ⇒ DrawBatch 记账的 maxDrawCount 是**真上界**，不会截绘制。
        // 交接帧（上帧接管、本帧前提破了）：批全项但不接管 = 一帧过画（屏内观感
        // 无损、guard 已报错），下一帧 cull 恢复。shadow pass 同样跳（caster 全进
        // 阴影图 = 多画不出错，首版口径，见设计稿 §8.3）。
        if (world && IsGPUCullFullTakeoverActive())   // T3i：只有全量接管才跳过 CPU 剔除
            return;

        const uint64_t t0 = GetUptimeUs();
        PerformFrustumCulling();
        if (s_cull_timer.Active())
            s_cull_timer.Add(DeltaUs(t0));
    }

    void PrimitiveBatchPipeline::RunBatching()
    {
        static PerfTimer s_batch_timer("batching");

        const uint64_t t0 = GetUptimeUs();
        BuildMaterialBatches();
        FinalizeBatches();
        if (s_batch_timer.Active())
            s_batch_timer.Add(DeltaUs(t0));
    }

    void PrimitiveBatchPipeline::PerformFrustumCulling()
    {
        if (!world || !camera_info)
            return;

        auto& cache = world->GetRenderFrameCache();

        frustum.SetMatrix(camera_info->vp);

        for (auto& itemPtr : cache.renderItems)
        {
            RenderItem* item = itemPtr.get();
            if (!item || !item->isVisible)
                continue;

            auto entity = item->GetEntity();
            if (!entity)
                continue;

            const BoundingBoxAccessor bbox = world->GetBoundingBoxByEntity(entity->GetEntityID());

            if (bbox.IsValid())
            {
                if (bbox.HasWorldBounds())
                    item->isVisible = TestFrustumWithWorldAABB(item, bbox.GetWorldBounds());
                else
                    item->isVisible = TestFrustumWithLocalAABB(item, bbox.GetCenter(), bbox.GetExtents());
            }
            else
            {
                item->isVisible = TestFrustumWithBoundingSphere(item);
            }
        }
    }

    bool PrimitiveBatchPipeline::TestFrustumWithWorldAABB(RenderItem* item, const math::AABB& world_aabb)
    {
        //worldAABB 已包含实体缩放，可直接用 P/N 顶点法做精确判定
        return frustum.BoxIn(world_aabb) != math::Frustum::Scope::OUTSIDE;
    }

    bool PrimitiveBatchPipeline::TestFrustumWithLocalAABB(RenderItem* item, const glm::vec3& local_center, const glm::vec3& local_extents)
    {
        const glm::mat4 worldMat = item->GetWorldMatrix();
        const glm::vec3 world_center = glm::vec3(worldMat * glm::vec4(local_center, 1.0f));

        //本地尺寸必须乘以世界缩放，否则放大的实体会被误剔除
        const float radius = ToWorldBoundingRadius(glm::length(local_extents), worldMat);

        return frustum.SphereIn(world_center, radius) != math::Frustum::Scope::OUTSIDE;
    }

    bool PrimitiveBatchPipeline::TestFrustumWithBoundingSphere(RenderItem* item)
    {
        auto* prim_item = dynamic_cast<PrimitiveRenderItem*>(item);
        if (!prim_item)
            return true;  // No bounding sphere data for this item type — keep visible
        Entity *owner = item->GetEntity();
        if (!owner)
            return false;

        auto transform = item->GetTransform();
        if (!transform.IsValid())
            return false;

        // A5a：包围球半径随几何状态迁入 GeometryData 组件（实体级查询，不在组件内缓存）
        const GeometryData *geometry = nullptr;
        if (ECSContext *context = owner->GetContext())
            geometry = context->GetGeometryData(owner);

        const float boundingRadius = geometry ? geometry->GetBoundingRadius() : 0.0f;

        if (boundingRadius <= 0.0f)
            return true;

        const glm::vec3 worldPos = transform.GetWorldPosition();

        //GetBoundingRadius() 由几何体本地 AABB 求得，不含实体缩放，这里补上世界缩放
        const float worldRadius = ToWorldBoundingRadius(boundingRadius, item->GetWorldMatrix());

        return frustum.SphereIn(worldPos, worldRadius) != math::Frustum::Scope::OUTSIDE;
    }


    graph::BufferManager* PrimitiveBatchPipeline::GetBufferManager() const
    {
        // W7：GetGraphicsContext() 已内置 render_ctx fallback
        auto *gc = world ? world->GetGraphicsContext() : nullptr;
        return gc ? gc->GetBufferManager() : nullptr;
    }

    void PrimitiveBatchPipeline::FinalizeBatch(MaterialBatch& batch)
    {
        // 本批次的**世界表**（WorldAddresses）本帧槽地址：表内各列（相机行 / 实体 4-ID 表 / 光源行 …）
        // 都是世界私有地址 —— 多世界渲染只换这一个指针。表地址随世界、随帧槽 ⇒ 每批重取。
        batch.world_addresses_address = world
            ? world->GetWorldAddressesAddress(world->GetFrameIndex()) : 0;

        for (auto *item : batch.items)
        {

            if (auto *prim_item = dynamic_cast<PrimitiveRenderItem *>(item))
            {
                // A4：材质运行期绑定在世界的**共享行**上（RenderItem 只持行号）。
                if (const MaterialRuntimeRow *mat_row = prim_item->GetMaterialRuntimeRow())
                {
                    if (mat_row->material_texture_zero_row_gpu)
                        batch.texture_reference_base_addr = mat_row->material_texture_zero_row_gpu;
                    else if (mat_row->material_texture_row_gpu)
                        batch.texture_reference_base_addr =
                            mat_row->material_texture_row_gpu -
                            uint64_t(mat_row->material_texture_configuration.row_index) *
                            uint64_t(mat_row->material_texture_configuration.row_stride);
                }
            }
        }

        // 批级纹理引用基址诊断（ULRE_DEBUG.toml 的 channels.binding = true）：「纹理不生效」的第一现场——
        // base==0 ⇒ push 出去的 addr_texture_references 是 0 ⇒ 着色器 MTL_TEX 解引用
        // 地址 0 ⇒ tex_handle=0 ⇒ SampleOptional 走 fallback 1.0（alpha 恒 1，
        // dither/blend 全部看不出效果）。
        if (ULRE_DEBUG_ON(Binding))
        {
            static uint32_t s_texture_base_reported = 0;

            if (s_texture_base_reported < 12)
            {
                ++s_texture_base_reported;

                GLogInfo("[BatchTextureRef] batch_items=%u base=0x%llx",
                         uint32_t(batch.items.size()),
                         static_cast<unsigned long long>(batch.texture_reference_base_addr));
            }
        }
    }

    void PrimitiveBatchPipeline::BuildMaterialBatches()
    {
        if (!world)
            return;

        auto& cache = world->GetRenderFrameCache();

        TransformAssignmentBuffer* shared_transform_buffer = nullptr;
        if (auto transform_system = world->GetSystem<TransformSystem>())
        {
            shared_transform_buffer = transform_system->GetTransformBuffer();
        }

        auto buffer_manager = GetBufferManager();
        // 当前渲染目标唯一权威：world->GetRenderTarget()（RenderContext 副本已删除）
        auto* rt = world ? world->GetRenderTarget() : nullptr;
        auto* current_render_pass = rt ? rt->GetRenderPass() : nullptr;

        for (auto& itemPtr : cache.renderItems)
        {
            RenderItem* item = itemPtr.get();
            if (!item || !item->isVisible)
                continue;

            auto* shader_prog = item->GetShaderProgram();
            auto* pipeline = item->GetPipeline(current_render_pass);
            auto* prim_item = dynamic_cast<PrimitiveRenderItem*>(item);
            const MaterialRuntimeRow *material_row = prim_item ? prim_item->GetMaterialRuntimeRow() : nullptr;

            if (prim_item)
            {
                const bool needs_recipe_rows = shader_prog
                    && shader_prog->RequiresRecipeRuntimeRows();
                const bool missing_rows = needs_recipe_rows
                                       && (!material_row
                                        || material_row->data_index_row == uint32_t(-1));
                if (missing_rows)
                {
                    LogWarning("[PrimitiveBatchPipeline] Skip primitive item: unresolved recipe rows. shader_prog=%s",
                               shader_prog ? shader_prog->GetName().c_str() : "<null>");
                    continue;
                }
            }

            if (!shader_prog || !pipeline)
            {
                // A7b：「有无材质来源」不再用于**静默**这条失败告警 —— 无材质来源的图元
                // 现在会走回退（错误）材质进入批次，其管线解析失败同样必须留痕，不能因为
                // 它“本来没有材质”就无声跳过（这正是被用户拍板取消的那条旧语义）。
                LogWarning("[PrimitiveBatchPipeline] Skip primitive item: unresolved runtime pipeline. shader_prog=%s render_pass=%p",
                           shader_prog ? shader_prog->GetName().c_str() : "<null>",
                           current_render_pass);
                continue;
            }

            // EDS 1/2/3：管线对象只由 Shader Stages + FBO 附件格式决定，**不含**渲染状态
            // ⇒ 键必须再带上「本实例在本 pass 下解析出的动态渲染状态」，否则双面/单面、
            // 混合/不透明等形态不同的图元会落进同一批，后者套用前者的状态（历史 bug）。
            ShaderProgramPipelineKey key(shader_prog,
                                         pipeline,
                                         prim_item
                                            ? prim_item->GetPipelineConfig(current_render_pass)
                                            : graph::mtl::MaterialPipelineConfig{});
            auto* batch_ptr = cache.materialBatches.GetValuePointer(key);

            if (!batch_ptr)
            {
                auto batch = std::make_unique<MaterialBatch>(key, device, buffer_manager);
                batch->transform_buffer = shared_transform_buffer;
                batch->AddItem(item);
                cache.materialBatches[key] = std::move(batch);
            }
            else
            {
                (*batch_ptr)->buffer_manager = buffer_manager;
                (*batch_ptr)->transform_buffer = shared_transform_buffer;
                (*batch_ptr)->AddItem(item);
            }
        }
    }

    void PrimitiveBatchPipeline::TryApplyGPUCullOverride()
    {
        static PerfTimer s_guard_timer("guard_hook");
        const PerfSpan guard_span(s_guard_timer);   // RAII：多点 return 出口统一累计

        auto& cache = world->GetRenderFrameCache();

        // 每帧先复位（评估失败/从未评估 ⇒ 恒不接管；collect 读本记账）
        ++gpu_cull_override_eval_frame_;

        // A6-d T2：**接管已强制**（opt-in 面整体删除）——主帧 mesh 批恒走 GPU 路径，
        // 不再有「未启用就不接管」的活口；本函数只剩「系统在不在」这一道检查。
        auto gpu_cull = world->GetSystem<GPUCullSystem>();
        if (!gpu_cull)
            return;

        // 延迟初始化与**首次批装配**同帧：guard 相位（RenderBatch）早于系统的 Update
        // 相位（RenderFrameSync）⇒ 不在此确保，新世界首帧的标签/布局写入会拿到空缓冲，
        // 布局永不落、该世界永不画（RTT 两例根因；每帧驱动的世界则只闪首帧、无人报故）。
        if (!gpu_cull->EnsureInitialized())
            return;

        // T4c3b/c：**两 pass 都可接管**（早期退已删）。
        // T4c3 首试失败的根因不在"两 pass 接管"本身，而在**行内容并非 pass 无关**：
        // 4-ID 表（transform/geometry/material/texture）由 collect 按 pass 写，而 gather
        // 当时只读主帧那一半 ⇒ 阴影行带的是主帧的材质变体（阴影程序按 pass 解析材质
        // ⇒ 遮罩/变体用错 ⇒ ATS 契约破、Cascade 挂）。T4c3a 已把 4-ID 表按 pass 分半，
        // 本批把 gather 的读点也按 pass 取（见 `entity_gpu_cull.glsl` 的 `ids_blk`）。
        //
        // 每 pass 自治：各写各的**块**（`pass*kBatchBlockStride`）、各占各的**竞技场扇区**
        //（`ArenaSectorBase(pass)`）、各读各的 4-ID 半、各记各的**切片**（绘制侧按当前
        // pass 取 `MaterialBatch::gpu_slice[pass]`）。自洽性依据：派发本就**每 RT 一次**
        //（主帧槽 + 阴影离屏槽），每槽的布局只由"该槽自己的那次批装配"写 ⇒ 行、标签、
        // 计数、切片全在同槽内闭环，不存在跨槽读取。
        const uint32_t pass_index = world->IsCurrentPassShadow() ? RenderPassSlot::Shadow
                                                                 : RenderPassSlot::Main;

        // ── T3i（③ 多批）：逐批评估 + 共享竞技场切片装配 ──────────────────────
        // 迭代顺序 = `cache.materialBatches` 的 map 键序（确定性）⇒ 批号帧间稳定；
        // 未接管的批**标签留无效** ⇒ cull 不为其写行、CPU 路径照旧绘制（按批粒度：
        // 一批不满足前提不牵连其余批）——单次派发 + 标签设计白拿的粒度。
        auto *gc = world->GetGraphicsContext();
        auto *pool = gc ? gc->GetGlobalSSBOBufferRegistry() : nullptr;
        const EntityRenderIDTable &rid_table = world->GetEntityRenderIDTable();

        // A6-d T4b：拒 = **fail-fast**（不再回 CPU 账兜底）。依据 `GPUCullSystem::kMaxBatches`
        // 的拍板政策：容量是冻结值，超限 = 项目 bug ⇒ 不扩容、不静默丢批；也不许"退回 CPU 账"
        // 把问题伪装成正常渲染。
        // 批 1.3 C5：闩改**每实例**（原先的进程级 static 告警闩 ⇒ 第二个世界静默）
        // ⇒ 该 lambda 必须捕获 this。
        auto reject = [this](const char *why, const char *batch_name, MaterialBatch *b)
        {
            if (!gpu_cull_reject_warned_)
            {
                gpu_cull_reject_warned_ = true;
                GLogError("[T3i] GPU cull draw override 该批前提不满足：%s（批=%s）"
                          "⇒ fail-fast：该批本帧**不绘制**（容量/前提为拍板冻结值，超限属项目 bug；"
                          "不回 CPU 账、不静默丢批）",
                          why, batch_name ? batch_name : "<null>");
            }

            // 「不绘制」的落法 = 清空批内项：下游（FinalizeBatch 的排序/聚簇，以及两个绘制
            // 系统的 `items.empty()` 早退）都以空批跳过 ⇒ 不发绘制命令（CPU 侧的 ICB / 行表
            // 构建链已随 2026-10-14 批 D 整族删除）。标签留无效 ⇒ cull 也不为本批写行。
            if (b)
                b->items.clear();
        };

        /// T5：待接管的批（**一个批 = 若干连续桶 + 一段连续实例行**）
        struct PendingBatch
        {
            MaterialBatch *batch        = nullptr;
            uint32_t       bucket_first = 0;   ///<首个桶在本 pass 块内的编号
            uint32_t       bucket_count = 0;   ///<桶数（= 本批命令条数）
            uint32_t       inst_base    = 0;   ///<实例行起点（arena 绝对行号）
            uint32_t       inst_rows    = 0;   ///<实例行数（= Σ 桶容量，含 4 行对齐浪费）
        };

        PendingBatch pending[GPUCullSystem::kMaxBatches];
        uint32_t pending_count = 0;

        uint32_t next_index = 0;
        // 诊断接线（`ULRE_DEBUG.toml` 的 `channels.batch/cull = true`）：逐帧/逐 pass 记录接管评估的出口与关键量——
        // "某帧 count=0"这类症状必须靠它定位（帧摘要只看一张图，看不见帧间差异）。
        const bool s_takeover_trace = ULRE_DEBUG_ON(Batch);
        // T4c3c：切片起点 = 本 pass 的竞技场扇区（主帧 0 / 阴影 4096）⇒ 两 pass 行集互不覆盖。
        uint32_t row_cursor = GPUCullSystem::ArenaSectorBase(pass_index);
        uint32_t total_items = 0;
        uint32_t tagged_items = 0;   // 诊断：本帧打上标签的项数
        uint32_t geometry_batches = 0;
        uint32_t *tags = nullptr;

        // T5 素材桶草稿（**跨批复用**，避免每批每帧堆分配）：桶表 + 项→桶映射。
        // 桶数 = 本批出现过的几何种类数（典型个位数）；线性查表省掉 map 的分配。
        std::vector<uint32_t> bucket_geometry;    // 桶 → geometry_id
        std::vector<uint32_t> bucket_capacity;    // 桶 → 容量（= 该几何体的项数）
        std::vector<uint32_t> item_bucket;        // 项 → 桶下标（打标签用）

        // A/B 诊断开关（临时）：`ULRE_NO_INSTANCING=1` ⇒ 每项独立成桶（capacity=1），
        // 等价于改造前的"每项一条单实例命令"。用于与实例化路径做逐像素对拍。
        static const bool s_no_instancing = (getenv("ULRE_NO_INSTANCING") != nullptr);

        for (auto& pair : cache.materialBatches)
        {
            MaterialBatch *b = pair.second.get();

            if (!b || b->items.empty() || !BatchRequiresIndirectCommands(*b))
                continue;

            ++geometry_batches;

            const char *batch_name = b->key.shader_program
                ? b->key.shader_program->GetName().c_str() : "<null>";

            // 逐批前提（一条不满足 ⇒ **仅该批**不接管）
            //
            // A6-c：原「批无 mesh stage」拒绝条件已删——它是**重复前提**：mesh 是本引擎唯一
            // 的顶点路径（见 `BatchRequiresIndirectCommands` 注释）⇒ 能进行到这里的批必然有
            // mesh stage（全家族台架零命中，2026-10-05）。
            //（T4c4 起那条"写不出 mesh 命令"的兜底已随 CPU 命令链删除；2026-10-14 批 D 又删掉
            //  本类里同名的那套 mesh per-draw 参数表构建 —— 两者都已不在。）
            const char *why = nullptr;

            // ── T5（实例化渲染）：批内按 `geometry_id` 聚类成**素材桶** ─────────────
            // 同批内同 `geometry_id` 的项共享**一条实例化 draw**（`groupCountY` = 该桶
            // 幸存数）。布局行的最小单位由此变成"桶"而不是"整批" ⇒ 一个 MaterialBatch
            // 占若干连续布局行；绘制侧按 `gpu_slice[pass].cmd_base/cmd_count` 发
            // `cmd_count` 条命令。
            //
            // 聚类是 CPU 端**确定性**的（不依赖 GPU 剔除结果）：桶集合 = 本批出现过的
            // 几何体种类，帧间稳定（同一 `materialBatches` 键序 + 同一批项集）⇒ 桶号
            // 稳定、标签稳定、命令表稳定。
            bucket_geometry.clear();
            bucket_capacity.clear();

            if (next_index >= GPUCullSystem::kMaxBatches)
                why = "接管素材桶数超 kMaxBatches(1024)";
            else
            {
                const size_t item_count = b->items.size();

                item_bucket.resize(item_count);

                for (size_t i = 0; i < item_count; ++i)
                {
                    RenderItem *item = b->items[i];

                    if (!item)
                    {
                        why = "批内含空项";
                        break;
                    }

                    // A6-c：原「含无 owner 实体项」拒绝条件已删——现管线只产带**有效
                    // EntityID** 的 `PrimitiveRenderItem`（无 owner 项属已退役的外部注入面）
                    // ⇒ 无生产者（全家族台架零命中）。真出现无 owner 项，下面的解引用会当场
                    // 崩（响亮），好过静默丢项。
                    Entity *owner = item->GetEntity();

                    const graph::RenderItemDescriptor *ids =
                        rid_table.Row(pass_index, owner->GetEntityID().index);

                    if (!ids || ids->geometry_id == 0)
                    {
                        why = "实体 4-ID 行缺失";
                        break;
                    }

                    const auto *params =
                        pool ? pool->GetMeshDrawParamsRow(ids->geometry_id) : nullptr;

                    if (!params)
                    {
                        why = "MeshDrawParams 池行缺失";
                        break;
                    }

                    // 线性扫已建桶：桶数 = 本批几何种类数（典型个位数）⇒ 比对次数
                    // 远小于每次建 map 的开销，且**无额外分配**。
                    uint32_t bucket = UINT32_MAX;

                    if (!s_no_instancing)
                        for (uint32_t k = 0; k < uint32_t(bucket_geometry.size()); ++k)
                            if (bucket_geometry[k] == ids->geometry_id)
                            {
                                bucket = k;
                                break;
                            }

                    if (bucket == UINT32_MAX)
                    {
                        bucket = uint32_t(bucket_geometry.size());
                        bucket_geometry.push_back(ids->geometry_id);
                        bucket_capacity.push_back(0u);
                    }

                    ++bucket_capacity[bucket];
                    item_bucket[i] = bucket;
                }

                if (!why
                 && next_index + uint32_t(bucket_geometry.size()) > GPUCullSystem::kMaxBatches)
                {
                    why = "接管素材桶数超 kMaxBatches(1024)";
                }
                else if (!why)
                {
                    // 竞技场容量前置校验（实例行连续占用 = Σ 桶容量）：超了就是装配 bug，
                    // 拒绝整批（fail-fast），不许写出越界行。
                    const uint32_t sector_begin = GPUCullSystem::ArenaSectorBase(pass_index);
                    const uint32_t sector_end   = sector_begin + GPUCullSystem::kArenaRowsPerPass;

                    uint32_t need_rows = uint32_t(bucket_geometry.size());   // ≥1 行/桶

                    for (uint32_t cap : bucket_capacity)
                        need_rows += cap;

                    if (row_cursor + need_rows > sector_end)
                        why = "竞技场实例行超容量（kArenaRowsPerPass）";
                }
            }

            if (why)
            {
                reject(why, batch_name, b);
                continue;
            }

            // 首个可接管批时取标签数组（进入即整表无效初值）
            if (!tags)
            {
                tags = gpu_cull->BeginBatchTagWrite(pass_index);
                if (!tags)
                {
                    // 响亮（A6-d T4b 政策：不许静默丢接管）：Map 失败 = 设备/资源问题
                    GLogError("[T3i] GPU cull draw override：BeginBatchTagWrite 拿不到标签映射"
                              "（世界=%s pass=%u）⇒ 本帧不接管",
                              world->GetName().c_str(), pass_index);
                    return;
                }
            }

            const uint32_t bucket_total = uint32_t(bucket_geometry.size());
            const uint32_t bucket_first = next_index;
            const uint32_t inst_base    = row_cursor;

            // A6 前置：把本批的 dispatch 档随行下发（gather 按档选 X 公式）——
            // 线的 X 按段数、组大小 64，其余按顶点数、组大小 192，两值都出自
            // `MeshDispatchGroupSize()` 单一真源（CPU 的 ICB 组装同源 ⇒ 两条账不分叉）。
            // 同批同 shader ⇒ 档与组大小对整批所有桶同值。
            const bool batch_is_lines =
                b->key.shader_program->GetPrimitiveType() == graph::PrimitiveType::Lines;
            const uint32_t topology = batch_is_lines
                ? GPUCullSystem::kBatchTopologyLines
                : GPUCullSystem::kBatchTopologyTriangles;
            const uint32_t group_size = MeshDispatchGroupSize(batch_is_lines);

            for (uint32_t k = 0; k < bucket_total; ++k)
            {
                const uint32_t capacity = bucket_capacity[k];

                // 布局行 = 一个桶：`row_base` 是**实例行起点**（gather 写 l2w/mtl 用），
                // capacity = 桶容量（溢出判据 + 台架走查上界）。
                gpu_cull->SetBatchLayout(pass_index, next_index, row_cursor, capacity,
                                         topology, group_size);
                ++next_index;

                // T3i：切片起点按 **4 行对齐** —— 绘制侧把 push 地址加 `row_base × 行字节`
                // 后仍须满足各引用声明的 16B 对齐（l2w 4B/行、mtl 16B/行）
                // ⇒ row_base % 4 == 0 是同时满足两者的条件（每桶至多浪费 3 行）。
                row_cursor = (row_cursor + capacity + 3u) & ~3u;
                total_items += capacity;
            }

            for (size_t i = 0; i < b->items.size(); ++i)
            {
                Entity *owner = b->items[i]->GetEntity();
                // 结构类值：本帧装配、本帧消费。标签 = **桶号**（该实体所属几何桶）。
                tags[owner->GetEntityID().index] = bucket_first + item_bucket[i];
            }

            if (s_takeover_trace)
                tagged_items += uint32_t(b->items.size());   // 诊断：本帧本 pass 打上标签的项数

            // 诊断（`ULRE_DEBUG.toml` 的 `channels.batch = true`）：本批的桶构成。
            // 实例化是否把某些项并进同一条命令，只能从这里看；没有它就只能靠像素差反推
            // （而带旋转 gizmo 的示例根本无法跨运行对比），排查成本极高。
            if (s_takeover_trace)
            {
                AnsiString bucket_list;

                for (uint32_t k = 0; k < bucket_total; ++k)
                {
                    if (k)
                        bucket_list += ",";
                    bucket_list += AnsiString::numberOf(bucket_capacity[k]);
                    bucket_list += "x";
                    bucket_list += AnsiString::numberOf(bucket_geometry[k]);
                }

                GLogInfo(u8"[T5][BUCKETS] 批=%s items=%u 桶=%u [%s] 档=%u 组大小=%u",
                         batch_name, uint32_t(b->items.size()), bucket_total,
                         bucket_list.c_str(), topology, group_size);
            }

            pending[pending_count].batch        = b;
            pending[pending_count].bucket_first = bucket_first;
            pending[pending_count].bucket_count = bucket_total;
            pending[pending_count].inst_base    = inst_base;
            pending[pending_count].inst_rows    = row_cursor - inst_base;
            ++pending_count;
        }

        if (pending_count == 0)
        {
            if (tags)
                gpu_cull->EndBatchLayout(pass_index, 0);   // 无接管批 ⇒ 本 pass 批数=0
            if (s_takeover_trace)
                GLogInfo("[T3i][TRACE] 世界=%s frame=%u pass=%u 出口=pending0 几何批=%u 辖批=%u items=%u arena_rows=%u",
                         world ? world->GetName().c_str() : "<null>",
                         world ? world->GetFrameIndex() : 0u, (world && world->IsCurrentPassShadow()) ? 1u : 0u, "pending0",
                         geometry_batches, pending_count, total_items, row_cursor);
            return;
        }

        // ⚠ 原两条定帧护栏已删（「IsDispatchSettled = 派发攒满 2 帧」与「槽首写不接管」），
        // 依据 = 它们是 **T3g/T4c-ini「不接管则退 CPU 账」时代的遗迹**：
        //   · T3i 起相位序为 RenderBatch(guard 写布局) → RenderFrameSync(cull 派发读布局)
        //     → RenderSubmit(绘制读计数)，**全部同帧闭环** ⇒ "布局必须此前某帧已被派发
        //     看过"不成立；垃圾布局的风险由 T4c-ini 建缓冲即填初值兜底
        //     （capacity==0 ⇒ shader 跳过该 pass，与未接管同口径）。
        //   · A6-d/T4c4 删净 CPU 账后，"本帧不接管"不再是退路而是**整批不画** ⇒
        //     旧护栏把"只渲染一次的离屏世界"（RTT 的 RenderToTexture/ColorDepth 走
        //     RenderOnce）永久锁死在 undsettled ⇒ 离屏 cmd 只剩 Begin/End。
        // 保留响亮出口：BeginBatchTagWrite 失败（资源问题）与逐批 guard 拒绝。

        // 分区/标签落设备（batch_count = 本 pass 的**素材桶**总数，T5 起语义变更）
        if (!gpu_cull->EndBatchLayout(pass_index, next_index))
            return;

        // 接管逐批（原 T4c-ini-2「槽首写不接管」已删，依据见上方护栏删除注释）。
        uint32_t engaged = 0;

        for (uint32_t i = 0; i < pending_count; ++i)
        {
            if (gpu_cull->ApplyGPUCullOverrideHook(pending[i].batch,
                                                   pending[i].bucket_first,
                                                   pending[i].bucket_count,
                                                   pending[i].inst_base,
                                                   pending[i].inst_rows,
                                                   pass_index))
                ++engaged;
        }

        if (engaged == 0)
            return;

        // 记账：任一批接管 ⇒ 本帧"接管中"；**全部**含几何批接管 ⇒ "全量接管"。
        // 只有全量才允许跳过 CPU 剔除（RunCulling 读上一帧结果）：部分接管时
        // CPU-driven 批仍需 CPU 剔除，否则它们会退化成"全项提交"。
        gpu_cull_override_frame_ = gpu_cull_override_eval_frame_;

        if (engaged == geometry_batches)
            gpu_cull_full_frame_ = gpu_cull_override_eval_frame_;

        // 正向判据日志（一次性）：证明"绘制真走 gather 表"（T4b 起已无 CPU 账可退，
        // 本日志仍是"接管真发生"的正面证据）。逐桶计数由台架在 cull 之后读。
        if (s_takeover_trace)
            GLogInfo("[T5][TRACE] 世界=%s frame=%u pass=%u 出口=ok 槽=%u 几何批=%u 辖批=%u 桶=%u items=%u arena_rows=%u "
                     "标签项=%u（应 == items）",
                     world ? world->GetName().c_str() : "<null>",
                     world ? world->GetFrameIndex() : 0u, (world && world->IsCurrentPassShadow()) ? 1u : 0u,
                     (world ? world->GetFrameIndex() : 0u) % HGL_FRAME_SLOT_TOTAL, geometry_batches,
                     pending_count, next_index, total_items, row_cursor, tagged_items);

        if (!gpu_cull_engaged_logged_)
        {
            gpu_cull_engaged_logged_ = true;
            GLogInfo("[T5] GPU cull instanced draw ENGAGED: 世界=%s batches=%u/%u buckets=%u items=%u"
                     " arena_rows=%u（实例化：每条命令承载整桶实例）",
                     world ? world->GetName().c_str() : "<null>",
                     engaged, geometry_batches, next_index, total_items, row_cursor);
        }
    }

    void PrimitiveBatchPipeline::FinalizeBatches()
    {
        if (!world)
            return;

        auto& cache = world->GetRenderFrameCache();

        // T3f-2（形态 B）：接管判定先于任何 FinalizeBatch —— hook 成功 ⇒ 批带上
        // `gpu_driven_override`，后续 sort/ensure/build/write 走既有 GPU-Driven 跳过契约；
        // 失败 ⇒ hook 内部把批复位回 CPU 模式（幂等，防上一帧接管残留把本帧带进
        // "CPU 路径被跳、指针却指着 GPU 表"的坏帧）。
        TryApplyGPUCullOverride();

        for (auto& pair : cache.materialBatches)
        {
            if (pair.second)
                FinalizeBatch(*pair.second);
        }
    }
}
