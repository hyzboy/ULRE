#include<hgl/ecs/systems/render/collect/RenderCollectSystem.h>
#include "RenderCollectInternal.h"
#include<hgl/ecs/support/pipeline/RenderStrategyParity.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/pipeline/RenderResource.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/ShadowProxy.h>
#include<hgl/ecs/components/GPUModelRefComponent.h>
#include<hgl/ecs/support/pipeline/PrimitiveState.h>
#include<hgl/ecs/support/storage/FrameProfiler.h>
#include<cmath>
#include<hgl/ecs/support/material/MaterialRuntimeTable.h>
#include<hgl/ecs/support/material/MaterialRuntimePipelineTable.h>
#include<hgl/ecs/support/material/MaterialVariantTable.h>
#include<hgl/vk/FrameDebugger.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/ecs/core/render/PrimitiveRenderItem.h>
#include<hgl/ecs/core/render/ModelTemplateRenderItem.h>
#include<hgl/ecs/support/material/ModelTemplateMaterialTable.h>
#include<hgl/ecs/support/material/ModelTemplateBuilder.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include <cstdlib>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/render/buffer/RenderSceneUBOSystem.h>
#include<hgl/ecs/support/cull/VisibilityDataStorage.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/SSBOBufferRegistry.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/ModelTemplateRows.h>

#include<hgl/graph/ssbo/MaterialSSBOLayout.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/mtl/MaterialResolver.h>
#include<hgl/mtl/ScenePassProfileResolver.h>
#include<hgl/util/hash/FNV1a.h>
#include<hgl/common/DebugChannel.h>
#include<hgl/log/Log.h>
#include<hgl/vk/VKRenderPass.h>
#include<glm/glm.hpp>
#include<cstring>

namespace hgl::ecs
{
    using namespace hgl::ecs::detail::render_collect;

    namespace
    {
        // D9：阴影 pass 跳过路径的收敛参数（常量由 `RenderCollectInternal.h` 提供）。
        // 持续跳过（行未就绪/解析失败等）时原本每帧 bump 静态级联 revision →
        // 每帧全量重画且无任何日志。前 kShadowRetryFullBumpFrames 帧保持每帧 bump
        // （正常情况下一两帧内就收敛），超过后降频到每 kShadowRetryBumpPeriod 帧一次，
        // 并在跨越阈值时报一次错——既不刷屏也不放弃自愈。

        /// B3-iii：几何"就绪"的判据从"每帧武装每实体 CPU 几何副本"收敛为**行就位**
        /// （`geometry_id != 0`，即 `MeshDrawParams` 行已由几何创建期写入）。
        /// 无 asset / 无几何 ⇒ true（没有几何要渲染，与旧实现同语义）；行缺失则就地补一次再复读。
        bool EnsureGeometryRowReady(ECSContext *world,Entity *owner)
        {
            GeometryData *geometry_comp = FindGeometryOf(owner);
            if (!geometry_comp)
                return true;

            const auto *asset = geometry_comp->GetPrimitiveAsset();
            if (!asset || !asset->GetGeometry())
                return true;

            if (geometry_comp->GetGeometryID() != 0)
                return true;

            auto *gc = world ? world->Environment().GetGraphicsContext() : nullptr;
            auto *pool = gc ? gc->GetGlobalSSBOBufferRegistry() : nullptr;
            auto *dev = world ? world->Environment().GetGPUDevice() : nullptr;

            if (!pool || !dev)
                return false;

            const_cast<graph::Geometry *>(asset->GetGeometry())->EnsureMeshDrawParams(pool,dev);

            if (geometry_comp->GetGeometryID() == 0)
            {
                GLogError("[RenderCollectSystem] 几何行未就位：owner=%s",
                          GetOwnerName(owner));
                return false;
            }

            return true;
        }

        // ─────────────────────────────────────────────────────────────
        // A7a：收集判定的**唯一判据** = `RenderStrategyTable`（v2 §9.2 P2）。
        //
        // 事实（fact）由组件 / 世界回答，表只做"槽位 × 事实 ⇒ 需求"的组合：
        //   · 语义 predicate（CanRender / HasAnyMaterialSource / CanCastShadow /
        //     CanReceiveShadow / 实体级可见性）是**输入**，不是判据本身；
        //   · 世界态（当前是否阴影 pass / 是否落在阴影距离裁剪内）同样是输入。
        // 旧的手写 if 链已降为 `RenderStrategyParity` 的**参考实现**（Debug 每帧
        // 同输入对拍、不一致 GLogError；Release 整块编空），见两个收集循环里的
        // 反向守卫——"表驱动 = 旧链结果"因此可被实测证伪，而不是靠声明。
        // ─────────────────────────────────────────────────────────────
        struct CollectStrategy
        {
            uint32_t      needs           = 0;      ///< 表结论（需求位掩码）
            StrategyFacts facts;                    ///< 事实（诊断归因复用，不重复求值）
            bool          in_shadow_range = true;   ///< 阴影距离裁剪事实（参考实现复用）
        };

        /// 阶段 1：模板载体 —— 挂有效 `GPUModelRefComponent` 的整模型根实体。
        /// "能不能画"由模板池（本体部件行 + 材质绑定）决定，**不**依赖 GeometryData。
        bool IsModelTemplateCarrier(const Entity *entity)
        {
            if (!entity)
                return false;

            const GPUModelRefComponent *ref = entity->GetComponent<GPUModelRefComponent>();

            return ref && ref->GetTemplateID() != graph::ssbo::kInvalidModelTemplateID;
        }

        /// 实体 + 世界 ⇒ 收集策略（表是唯一判据）
        CollectStrategy EvaluateCollectStrategy(ECSContext *world,const Entity *owner)
        {
            CollectStrategy out;

            const bool template_carrier = IsModelTemplateCarrier(owner);

            out.facts.entity_visible      = owner ? world->Spatial().IsEntityVisible(owner->GetEntityID()) : false;
            out.facts.has_owner           = (owner != nullptr);
            out.facts.renderable          = template_carrier || CanRender(owner);
            out.facts.has_material_source = template_carrier || HasAnyMaterialSource(owner);
            out.facts.cast_shadow         = CanCastShadow(owner);
            out.facts.receive_shadow      = CanReceiveShadow(owner);
            out.facts.shadow_pass         = world->Runner().PassScope().IsShadowPass();

            if (out.facts.shadow_pass && owner)
            {
                const float max_dist = GetShadowMaxDistance(owner);

                if (max_dist > 0.0f && world->Runner().PassScope().HasShadowOrigin())
                {
                    const TransformAccessor transform =
                        world->Spatial().GetTransformByEntity(owner->GetEntityID());

                    if (transform.IsValid())
                    {
                        const glm::vec3 diff =
                            transform.GetWorldPosition() - world->Runner().PassScope().ShadowOrigin();

                        out.in_shadow_range = glm::dot(diff,diff) <= max_dist * max_dist;
                    }
                }
            }

            out.facts.in_shadow_range = out.in_shadow_range;
            out.needs = EvaluateRenderNeed(owner ? owner->BuildComponentMask() : 0u,
                                           out.facts.ToMask());

            return out;
        }
    }

    RenderCollectSystem::RenderCollectSystem(const std::string& name)
        : System(name)
    {
        // Set system type and properties
        SetExecutionPhase(ExecutionPhase::RenderCollect);
        SetRenderElementType("Primitive");

        // Declare dependencies
    }

    void RenderCollectSystem::PreparePrimitiveTemplates(
        const ValueArray<Entity *> &primitives, int active_mobility_filter,
        bool ambient_provider_changed, PreparationState &state)
    {
        auto &cache = world->Runner().FrameCache();
        MaterialRuntimeTable *runtime_table = world->RenderData().RuntimeMaterials();
        FrameProfileSpan prescan_span(FrameProfileSlot::CollectPreScan);

        for (Entity *entity : primitives)
        {
            if (!entity)
                continue;

            // A7a：收集判定 = 策略表（唯一判据）；事实由组件 / 世界回答。mobility 过滤是
            // 世界级过滤器（表不建模），仍在本循环里单独生效。
            const CollectStrategy strategy = EvaluateCollectStrategy(world,entity);

            Entity *owner = entity;
            if (strategy.facts.entity_visible && owner)
            {
                const EntityID owner_id=owner->GetEntityID();
                auto *trace=cache.GetOrCreateTrace(GetOwnerName(owner),owner_id.index,owner_id.generation);
                if (HasPendingGeometryResource(owner))
                {
                    state.any_pending_resources = true;
                    state.any_material_work     = true;
                    ++cache.pending_geometry_resources;
                    if(trace) trace->pending_geometry=true;
                }
                if (HasPendingMaterialResources(owner))
                {
                    state.any_pending_resources = true;
                    state.any_material_work     = true;
                    ++cache.pending_material_resources;
                    if(trace) trace->pending_material=true;
                }
            }

#if ULRE_STRATEGY_PARITY_ENABLED
            // ── 反向守卫：参考实现 = 手写 if 链（Release 下整块编空）───────────
            // 只有"表结论 = 参考实现结果"被每帧同输入实测过，表才算真接管了判据。
            // A7b：参考实现**已按新语义**去掉“有材质来源”这一条 —— 该条被用户拍板从
            // “收集判据”改为“材质错误（走回退材质）”，故它不再参与收集对拍，而是由下面
            // 独立的 ParityCheckFallbackMaterial 逐帧对拍。这不是静默放水：把无来源改回
            // 剔除、或把回退规则写歪，都会在那条守卫上立刻报不一致。
            {
                const bool legacy_collect =
                       world->Spatial().IsEntityVisible(entity->GetEntityID())
                    && (CanRender(entity) || IsModelTemplateCarrier(entity))
                    && (entity != nullptr)
                    && (!strategy.facts.shadow_pass
                        || (CanCastShadow(entity) && strategy.in_shadow_range));

                ParityCheckCollect(HasRenderNeed(strategy.needs,RenderNeed::CollectForCurrentPass),
                                   legacy_collect,
                                   "RenderCollectSystem#prescan");

                ParityCheckShadowCaster(HasRenderNeed(strategy.needs,RenderNeed::ShadowCaster),
                                        CanCastShadow(entity),
                                        "RenderCollectSystem#prescan");

                // A7b 新语义守卫：表说“材质缺失 ⇒ 回退材质” ⟺ 事实“无任何材质来源”。
                ParityCheckFallbackMaterial(HasRenderNeed(strategy.needs,RenderNeed::FallbackMaterial),
                                            (!HasAnyMaterialSource(entity) && !IsModelTemplateCarrier(entity)),
                                            "RenderCollectSystem#prescan");
            }
#endif

            if (!HasRenderNeed(strategy.needs,RenderNeed::CollectForCurrentPass))
                continue;

            // 实体级可见性与 owner 已由表的 CollectForCurrentPass 要求 ⇒ 不再单独判据

            if (active_mobility_filter >= 0)
            {
                TransformAccessor transform = world->Spatial().GetTransformByEntity(entity->GetEntityID());
                if (!transform.IsValid() || static_cast<int>(transform.GetMobility()) != active_mobility_filter)
                    continue;
            }

            // 阶段 1：在进入收集主循环之前，确保所有有效渲染几何实体已绑定单图元模型模板
            if (ambient_provider_changed)
            {
                const auto *ref = entity->GetComponent<GPUModelRefComponent>();
                if (!ref || ref->IsAutomaticallyNormalized())
                    world->RenderData().GetOrCreateMaterialRuntimeSlot(entity->GetEntityID()).program_dirty = true;
            }
            if (!EnsurePrimitiveModelTemplate(entity))
            {
                state.any_material_work = true;
                state.any_pending_resources = true;
                state.failed_template_preparations.Add(entity);
            }

            // 阶段 1/2：模板载体实体（挂有效 GPUModelRefComponent）
            // 若在 ModelTemplateMaterialTable 中已注册绑定（自包含 Shading Groups），或无 GeometryData，
            // 则其部件几何与材质常驻 GPU SSBO，无需参与单图元 slot 预扫描。
            const GPUModelRefComponent *scan_model_ref = entity->GetComponent<GPUModelRefComponent>();
            if (scan_model_ref && scan_model_ref->GetTemplateID() != graph::ssbo::kInvalidModelTemplateID)
            {
                const ModelTemplateMaterialTable *template_mat_table = world->RenderData().TemplateMaterials();
                const bool has_template_binding = template_mat_table && template_mat_table->FindBinding(scan_model_ref->GetTemplateID()) != nullptr;
                if (has_template_binding || !entity->GetComponent<GeometryData>())
                    continue;
            }

            // 阴影 pass 的 caster 能力（CanCastShadow）与距离裁剪（in_shadow_range）已由表的
            // CollectForCurrentPass 条件要求表达 ⇒ 这里不再重复判据。

            // A4：材质运行期状态 = **每实例 slot**（世界表里按实体稀疏存放）+
            // **共享行**（绑定）。这里取/建 slot，再经行取 program 与缓存哈希。
            MaterialRuntimeSlot &material_slot =
                world->RenderData().GetOrCreateMaterialRuntimeSlot(entity->GetEntityID());

            // 天光算法热切：provider 变 ⇒ program 身份变，快路径必须失效（见 Update 开头）。
            if (ambient_provider_changed)
                material_slot.program_dirty = true;

            const MaterialRuntimeRow *material_row = runtime_table
                ? runtime_table->Get(material_slot.row)
                : nullptr;

            const graph::ShaderProgram *forward_program =
                GetForwardProgram(GetVariantTable(world), material_row);

            const bool runtime_rows =
                forward_program
             && forward_program->RequiresRecipeRuntimeRows();

            // A primitive whose program is not yet resolved may still resolve
            // to a runtime-rows program this frame, so it can trigger direct
            // recipe materialization. Treat it as a possible runtime-rows
            // primitive.
            const bool possible_runtime_rows =
                runtime_rows || !forward_program;

            const bool fast_path_holds =
                   !material_slot.program_dirty
                && forward_program
                && material_slot.tracked_material_data_generation
                   == MaterialAuthoredGenerationOf(entity)
                && material_row
                && material_row->cached_effective_recipe_hash != 0;

            const bool epoch_stale =
                runtime_rows
             && material_slot.last_materialize_epoch != materialization.Epoch();

            // valid==true only survives a fully successful resolve+prepare+
            // materialize+geometry+pipeline chain, so a Failed material keeps
            // retrying every frame instead of being silently skipped.
            //
            // runtime_dirty is normally cleared at the end of a successful
            // materialize, so at pre-scan time a clean material has it false.
            // It can only be set here if the generation advanced in the middle
            // of the previous frame's loop (a race that leaves it unconsumed)
            // — forcing it into needs_work makes the next frame re-run the full
            // chain so the flag gets consumed.
            const bool needs_work =
                !fast_path_holds || epoch_stale || !material_slot.valid
             || material_slot.runtime_dirty;

            state.any_material_work |= needs_work;
            state.any_possible_runtime_rows_visible |= possible_runtime_rows;
        }

    }

    void RenderCollectSystem::Update(float /*deltaTime*/)
    {
        if (!world)
            return;

        materialization.BeginFrame();
        if (auto *templates = world->RenderData().TemplateMaterials())
            templates->CollectRetiredTemplates();

        // 本 pass 生效相机的 CameraInfo（离屏 pass = pass 相机，主帧 = 主相机）：
        // 每帧现取，**不缓存指针**——pass 之间相机会换（旧实现把指针缓存在安装期，pass 相机一上场就错）。
        const graph::CameraInfo *cameraInfo = world->GetActiveCameraInfo();

        if (!cameraInfo)
            return;

        auto& cache = world->Runner().FrameCache();
        cache.cameraInfo = cameraInfo;
        hgl::graph::FrameDebugger::SetCameraSnapshot(cameraInfo);
        if(!world->Runner().PassScope().IsShadowPass())
            if(auto *swapchain=dynamic_cast<hgl::graph::SwapchainRenderTarget *>(world->Environment().GetRenderTarget()))
                swapchain->RecordCaptureCamera(cameraInfo);

        // P1：每 pass 一次资格判定（只统计，不改变本帧任何行为）。
        ObserveCollectCacheEligibility();

        // A7b：本 pass 回退（错误）材质配方合成次数（诊断；默认 0 ⇒ 不打印任何东西，
        // 基线日志逐字不变）。
        g_fallback_recipe_builds = 0;

        const int active_mobility_filter = world ? world->Runner().PassScope().GetMobilityFilter() : -1;

        // T2-c + 阶段 1：图元枚举源 = **GeometryData 活跃集 ∪ 纯模板载体实体**。
        //
        // 纯模板载体 = 挂 `GPUModelRefComponent` 但**不挂** `GeometryData` 的实体
        //（整模型根实体：部件几何/材质都在模板池里，CPU 侧没有任何几何副本）。
        // 旧实现只扫 GeometryData 池 ⇒ 这类实体永不入批（模板展开也就永远不执行）。
        ValueArray<Entity *> primitives;
        CollectPrimitiveEntities(world, primitives);

        // 天光算法热切兜底：ambient provider 名是 program 身份的一部分
        // （EnsureForwardProgram 把它折进 build context），但它不在材质授权代里，
        // 单看 slot.program_dirty / 授权代判不出"世界换了天光算法"，A3 快路径会
        // 直接返回旧 program。名字一变就把全体 slot 的 program 置脏，让每个实例都
        // 重算一次 build context。每帧代价 = 一次名字哈希比较。
        //
        // 只在**前向 pass** 比对并推进快照：depth/shadow 的 program 身份不含 provider
        // （`HashMaterialProgramBuildContext` 对 depth 目的不折入），若让阴影 pass 吃掉
        // 这次"变化"，它解析完会顺手把共享的 `program_dirty` 清回 false，前向 pass 就
        // 再也看不到 provider 变过 —— 表现为热切天光算法后前向材质仍用旧 program。
        bool ambient_provider_changed = false;
        if (!world->Runner().PassScope().IsShadowPass())
        {
            const AnsiString &ambient_provider_now = ResolveAmbientProviderModule(world);
            const uint64_t ambient_provider_hash =
                HashAmbientProviderModule(ambient_provider_now);

            ambient_provider_changed = materialization.UpdateAmbientProvider(ambient_provider_hash);
        }

        // P1-1: Global frame-level materialize gating.
        //
        // PrepareActivePlanResources and MaterializeRecipeRowsForPrimitive are
        // the per-frame hotspot: they re-traverse the resource plan, rebuild the
        // binding recipe and re-materialize rows on every Update even when
        // nothing changed. This pre-scan mirrors the resolve fast-path
        // (EnsureMaterialProgramForPrimitive), the materialization epoch and
        // the component success state to decide whether ANY primitive needs
        // work this frame. When none does, the main loop reuses last frame's
        // results and skips those two calls entirely.
        //
        // Epoch semantics: a materialize pass wipes per-primitive runtime rows
        // (rows are rebuilt from the recipe bindings / texture layer values
        // each time a primitive is materialized). A primitive skipped
        // this frame (e.g. invisible) therefore holds stale rows the moment any
        // other primitive materializes. The epoch is bumped in exactly those
        // frames so skipped primitives are re-flagged (epoch mismatch) when they
        // next render.
        PreparationState preparation;

        FrameProfiler::Count(FrameProfileCount::CollectPrimitives, primitives.GetCount());
        FrameProfiler::Count(FrameProfileCount::CollectCalls);

        cache.scene_primitives = static_cast<uint32_t>(primitives.GetCount());

        // P3：主帧收集快路径。命中即本 pass 的收集工作**全部完成** ⇒ 直接返回。
        // 未命中（任一前提不成立，或中途发现不一致而主动回退）⇒ 照原样跑完整收集，
        // 回退路径不留下任何半成品（见 `TryCachedMainFrameCollect` 的契约）。
        if (TryCachedMainFrameCollect(ambient_provider_changed))
            return;

        PreparePrimitiveTemplates(primitives, active_mobility_filter, ambient_provider_changed, preparation);

        if (preparation.any_material_work && preparation.any_possible_runtime_rows_visible)
            materialization.BumpEpoch();

        size_t skipped_invisible = 0;
        size_t skipped_no_owner = 0;
        size_t skipped_no_transform = 0;
        // S6 诊断：本 pass 产出图元的实体 ID 校验和（判定"条带帧 vs 整级帧是否同一批 caster"）
        uint64_t shadow_pass_idsum = 0;
        size_t added = 0;

        {
            FrameProfileSpan main_span(FrameProfileSlot::CollectMainLoop);

        for (Entity *entity : primitives)
        {
            if (!entity)
                continue;

            // A7a：收集判定 = 策略表（唯一判据）；事实由组件 / 世界回答。
            const CollectStrategy strategy = EvaluateCollectStrategy(world,entity);

#if ULRE_STRATEGY_PARITY_ENABLED
            // ── 反向守卫：参考实现 = 手写 if 链（Release 下整块编空）───────────
            // 只有"表结论 = 参考实现结果"被每帧同输入实测过，表才算真接管了判据。
            // A7b：参考实现**已按新语义**去掉“有材质来源”这一条 —— 该条被用户拍板从
            // “收集判据”改为“材质错误（走回退材质）”，故它不再参与收集对拍，而是由下面
            // 独立的 ParityCheckFallbackMaterial 逐帧对拍。这不是静默放水：把无来源改回
            // 剔除、或把回退规则写歪，都会在那条守卫上立刻报不一致。
            {
                const bool legacy_collect =
                       world->Spatial().IsEntityVisible(entity->GetEntityID())
                    && (CanRender(entity) || IsModelTemplateCarrier(entity))
                    && (entity != nullptr)
                    && (!strategy.facts.shadow_pass
                        || (CanCastShadow(entity) && strategy.in_shadow_range));

                ParityCheckCollect(HasRenderNeed(strategy.needs,RenderNeed::CollectForCurrentPass),
                                   legacy_collect,
                                   "RenderCollectSystem#collect");

                ParityCheckShadowCaster(HasRenderNeed(strategy.needs,RenderNeed::ShadowCaster),
                                        CanCastShadow(entity),
                                        "RenderCollectSystem#collect");

                // A7b 新语义守卫：表说“材质缺失 ⇒ 回退材质” ⟺ 事实“无任何材质来源”。
                ParityCheckFallbackMaterial(HasRenderNeed(strategy.needs,RenderNeed::FallbackMaterial),
                                            (!HasAnyMaterialSource(entity) && !IsModelTemplateCarrier(entity)),
                                            "RenderCollectSystem#collect");
            }
#endif

            const EntityID entity_id = entity->GetEntityID();
            const char *entity_name=GetOwnerName(entity);
            auto *trace=cache.GetOrCreateTrace(entity_name,entity_id.index,entity_id.generation);
            if(strategy.facts.renderable)
                ++cache.passed_can_render;

            if (!HasRenderNeed(strategy.needs,RenderNeed::CollectForCurrentPass))
            {
                // 判定已由表给出；下面只是 S6 探针的**归因**（不参与判定），口径与旧计数器
                // 一致：不可渲染者旧实现不计也不报，其后依次归因不可见 / 无 owner。
                // A7b：「无材质来源」**不再进这条分支** —— 它已不是剔除条件，而是材质错误
                // （走回退材质照常产出渲染项），对应的跳过计数与那条“无配方即跳过”的
                // 告警随之删除（禁复活由 Test 26 的禁复活 needle 钉住）。
                if (strategy.facts.renderable)
                {
                    if (!strategy.facts.entity_visible)
                    {
                        ++skipped_invisible;
                        ++cache.dropped_invisible;
                        if(trace)
                        {
                            ::strncpy(trace->collect_status,"filtered",sizeof(trace->collect_status)-1);
                            ::strncpy(trace->collect_reason,"invisible",sizeof(trace->collect_reason)-1);
                        }
                        cache.RecordDrop(entity_name,entity_id.index,"RenderCollect","Invisible","Entity is not visible");
                    }
                    else if (!strategy.facts.has_owner)
                    {
                        ++skipped_no_owner;
                        ++cache.dropped_no_owner;
                        if(trace)
                        {
                            ::strncpy(trace->collect_status,"filtered",sizeof(trace->collect_status)-1);
                            ::strncpy(trace->collect_reason,"no_owner",sizeof(trace->collect_reason)-1);
                        }
                        cache.RecordDrop(entity_name,entity_id.index,"RenderCollect","NoOwner","Renderable has no owner");
                    }
                    else
                    {
                        ++cache.dropped_other;
                        if(trace)
                        {
                            ::strncpy(trace->collect_status,"filtered",sizeof(trace->collect_status)-1);
                            ::strncpy(trace->collect_reason,"strategy_filtered",sizeof(trace->collect_reason)-1);
                        }
                        cache.RecordDrop(entity_name,entity_id.index,"RenderCollect","StrategyFiltered","Current pass strategy rejected primitive");
                    }
                }
                else
                {
                    ++cache.dropped_other;
                    if(trace)
                    {
                        ::strncpy(trace->collect_status,"filtered",sizeof(trace->collect_status)-1);
                        ::strncpy(trace->collect_reason,"not_renderable",sizeof(trace->collect_reason)-1);
                    }
                    cache.RecordDrop(entity_name,entity_id.index,"RenderCollect","NotRenderable","CanRender rejected primitive");
                }

                continue;
            }

            ++cache.collect_candidates;
            if(trace)
                ::strncpy(trace->collect_status,"candidate",sizeof(trace->collect_status)-1);

            // 实体级可见性与 owner 已由表的 CollectForCurrentPass 要求 ⇒ 不再单独判据

            TransformAccessor transform = world->Spatial().GetTransformByEntity(entity->GetEntityID());

            if (!transform.IsValid())
            {
                ++skipped_no_transform;
                ++cache.dropped_no_transform;
                if(trace)
                {
                    ::strncpy(trace->collect_status,"filtered",sizeof(trace->collect_status)-1);
                    ::strncpy(trace->collect_reason,"no_transform",sizeof(trace->collect_reason)-1);
                }
                cache.RecordDrop(entity_name,entity_id.index,"RenderCollect","NoTransform","Transform handle unavailable");
                continue;
            }

            if (active_mobility_filter >= 0 && static_cast<int>(transform.GetMobility()) != active_mobility_filter)
            {
                ++cache.dropped_mobility;
                if(trace)
                {
                    ::strncpy(trace->collect_status,"filtered",sizeof(trace->collect_status)-1);
                    ::strncpy(trace->collect_reason,"mobility_filtered",sizeof(trace->collect_reason)-1);
                }
                cache.RecordDrop(entity_name,entity_id.index,"RenderCollect","MobilityFiltered","Mobility does not match active pass");
                continue;
            }

            if (preparation.failed_template_preparations.Contains(entity))
            {
                ++cache.dropped_other;
                cache.RecordDrop(entity_name, entity_id.index, "RenderCollect",
                                 "TemplatePreparationFailed", "Model template or material preparation failed");
                if (trace)
                {
                    ::strncpy(trace->collect_status, "dropped", sizeof(trace->collect_status) - 1);
                    ::strncpy(trace->collect_reason, "template_preparation_failed", sizeof(trace->collect_reason) - 1);
                }
                continue;
            }

            // 阴影 pass 的 caster 能力与距离裁剪已由表的 CollectForCurrentPass 条件要求表达
            // ⇒ 这里不再重复判据（原两条 continue 随之删除）。

            if (CollectTemplateCarrier(entity, transform, added, preparation.any_material_work, shadow_pass_idsum))
                continue;

            // 未就绪实体等待后续帧重新归一化，不回退旧单图元收集路线。
            // 经过预扫描 EnsurePrimitiveModelTemplate，所有合法图元实体均已完成单图元模型模板晋升。
            // 若仍有非模板实体进入此处，记录 Drop 并跳过，不再保留旧单图元物化与收集分支。
            if (trace)
            {
                ::strncpy(trace->collect_status, "dropped", sizeof(trace->collect_status) - 1);
                ::strncpy(trace->collect_reason, "no_valid_model_template", sizeof(trace->collect_reason) - 1);
            }
            cache.RecordDrop(entity_name, entity_id.index, "RenderCollect", "NoModelTemplate", "Entity has no valid model template or binding");
            preparation.any_material_work = true;
            continue;
        }

        }// CollectMainLoop 子槽

        FrameProfiler::Count(FrameProfileCount::CollectItems, added);

        {
            FrameProfileSpan publish_span(FrameProfileSlot::CollectPublish);

        // S6 诊断（ULRE_DEBUG.toml 的 channels.shadow = true；默认静默）：阴影 pass 的收集结论。
        // 用途：判定"条带重画帧"与"整级重建帧"是否收到**同一批 caster**——
        //   数量/校验和不同 ⇒ 差异根因在收集/剔除侧（视锥、mobility 筛、shadow_origin 距离、
        //   程序 resolve 失败跳过）；
        //   完全相同 ⇒ 差异在光栅化侧（写侧平移矩阵、scissor、清除矩形、深度值）。
        // 校验和 = Σ entity_id（与顺序无关）。
        // A4：同时报材质运行期表的**行计数**（共享行 interned / CoW 自有行 / 每实例 slot）
        // —— 共享行数与独占行数就是这两项，探针与单测共用同一组 getter。
        if (g_fallback_recipe_builds > 0)
        {
            GLogWarning(u8"[MaterialFallback] shadow_pass=%d fallback material recipes built this pass: %u",
                        world->Runner().PassScope().IsShadowPass() ? 1 : 0, g_fallback_recipe_builds);
        }

        const bool s6_collect_log = ULRE_DEBUG_ON(Shadow);
        if (s6_collect_log && world && world->Runner().PassScope().IsShadowPass())
        {
            const MaterialRuntimeTable *runtime_stats = GetRuntimeTable(world);

            GLogInfo("[S6-COLLECT] shadow pass mobility=%d items=%zu idsum=0x%llx "
                     "skipped(invisible=%zu no_owner=%zu no_transform=%zu) "
                     "material_runtime(rows=%u shared=%u owned=%u slots=%u)",
                     active_mobility_filter, added,
                     static_cast<unsigned long long>(shadow_pass_idsum),
                     skipped_invisible, skipped_no_owner, skipped_no_transform,
                     runtime_stats ? runtime_stats->GetCount() : 0u,
                     runtime_stats ? runtime_stats->GetSharedRowCount() : 0u,
                     runtime_stats ? runtime_stats->GetOwnedRowCount() : 0u,
                     runtime_stats ? runtime_stats->GetSlotCount() : 0u);
        }

        }// CollectPublish 子槽

        // P3：记录本 pass 是否"稳态"（无任何图元需要完整材质链），供下一帧快路径判定。
        // 只在主帧 pass 更新：阴影/过滤 pass 的结论会污染这份判断。
        if (world->Runner().IsMainFrameTopologyPass())
        {
            collect_cache.SetLastMainCollectSteady(!preparation.any_material_work && !preparation.any_pending_resources);
            cache.is_collect_steady     = collect_cache.IsLastMainCollectSteady();
            cache.has_pending_resources = preparation.any_pending_resources;
        }
    }
}//namespace hgl::ecs
