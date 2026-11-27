#include <hgl/ecs/systems/render/collect/RenderCollectSystem.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/GPUModelRefComponent.h>
#include <hgl/ecs/components/ShadowProxy.h>
#include <hgl/ecs/support/pipeline/RenderStrategyParity.h>
#include <hgl/ecs/support/cull/VisibilityDataStorage.h>
#include <hgl/ecs/core/render/PrimitiveRenderItem.h>
#include <hgl/ecs/support/storage/MainFrameTopologySnapshot.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/ssbo/ModelTemplateRows.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    bool RenderCollectSystem::PublishPrimitiveFacts(const EntityID entity_id,
                                                            const uint32_t transform_id,
                                                            const uint32_t geometry_id,
                                                            const uint32_t material_id,
                                                            const uint32_t texture_id,
                                                            const uint32_t pass_index,
                                                            PrimitiveRenderItem *item,
                                                            const SnapshotItemRecord *draw_seed)
    {
        bool ids_published = false;
        bool has_draw_key  = false;

        // 阶段 1：模板载体（整模型根实体）**没有**代表几何体 ⇒ `has_draw_key` 恒 false，
        // 但它必须上行事实（模板 id / 部件掩码 / 4-ID 的材质纹理）并被批装配看见。
        // 判据 = 实体挂了有效 `GPUModelRefComponent`。
        bool is_template_carrier = false;
        Entity *carrier_owner = item ? item->GetEntity() : (world ? world->GetEntity(entity_id) : nullptr);
        if (carrier_owner)
        {
            const GPUModelRefComponent *model_ref =
                carrier_owner->GetComponent<GPUModelRefComponent>();

            is_template_carrier = (model_ref
                && model_ref->GetTemplateID() != graph::ssbo::kInvalidModelTemplateID);
        }

        if (item)
        {
            if (draw_seed)
            {
                // ── P3 快路径：绘制键直接取自拓扑快照 ────────────────────────────────
                // `MeshDrawParams` 的 CPU 行位于**映射的 GPU 缓冲**（host-visible 非缓存
                // 内存）⇒ 实测单个字段读取 ≈2.3 µs，每帧 1079×6 次 ≈14.6 ms（占整相位 90%+
                // 的唯一大头，已由差分实验定性）。绘制键只由池 `geometry_id` 决定 = 资产拓扑
                // ⇒ 拓扑未变（快照代际判据）时逐帧重现即可，**一个字段都不必再读**。
                // 记录里 `draw_geometry_id == 0` 与完整路径「params 行不成立」同义。
                if (draw_seed->draw_geometry_id != 0)
                {
                    RenderItem::MeshDrawKey &key = item->draw_key;
                    key.geometry_id  = draw_seed->draw_geometry_id;
                    key.vertex_base  = draw_seed->draw_vertex_base;
                    key.index_base   = draw_seed->draw_index_base;
                    key.vertex_count = draw_seed->draw_vertex_count;
                    key.index_count  = draw_seed->draw_index_count;
                    key.is_indexed   = draw_seed->draw_is_indexed;

                    has_draw_key = true;
                }
            }
            else if (geometry_id != 0)
            {
                auto *gc_key   = world ? world->GetGraphicsContext() : nullptr;
                auto *pool_key = gc_key ? gc_key->GetGlobalSSBOBufferRegistry() : nullptr;
                const auto *params_row = pool_key ? pool_key->GetMeshDrawParamsRow(geometry_id) : nullptr;

                if (params_row)
                {
                    RenderItem::MeshDrawKey &key = item->draw_key;
                    key.geometry_id  = geometry_id;
                    key.vertex_base  = params_row->vertex_base;
                    key.index_base   = params_row->index_base;
                    key.vertex_count = params_row->vertex_count;
                    key.index_count  = params_row->index_count;
                    key.is_indexed   = params_row->is_indexed;

                    has_draw_key = true;
                }
            }
        }

        // 绘制键成立 = 「本项能画」的同一判据点：4-ID 与全部事实都挂在这里上行。
        // 模板载体是唯一的例外：它没有绘制键，但"能画"由模板池决定 ⇒ 同样上行。
        if (has_draw_key || is_template_carrier)
        {
            // T3d-3a：实体 4-ID 表随"本项进批"同一判据上行
            //（消费者 = GPU 剔除 / CPU 批行写入 / 形态 B）
            graph::RenderItemDescriptor render_ids{};
            render_ids.transform_id = transform_id;
            render_ids.geometry_id  = geometry_id;
            render_ids.material_id  = material_id;
            render_ids.texture_id   = texture_id;

            world->GetEntityRenderIDTable().SetRenderIDs(pass_index, entity_id, render_ids);
            ids_published = true;

            // T3h：阴影**事实**随同一判据点上 EntityGPU 行（ShadowProxy 是
            // CPU-only 组件、无 GPU 行号可推 ⇒ 值进行才算接线；写者=世界，
            // 与 SetVisible 同族）。消费规则与着色侧逐字同式（bias<=0/非有限 ⇒ 行留 0
            // ⇒ 着色侧视 0 为默认 1.0）；剔除端按 `entity.shadow_*` 把本行抄进
            // per-draw 行（D3 契约段③ 现由这两跳承担 —— 原 CPU 行写入端已删）。
            {
                Entity *shadow_owner = world->GetEntity(entity_id);

                // A5b / A7a（2026-10-14 批 D **搬家**）：接收判定**由策略表给出**
                // —— `RenderNeed::ShadowReceiver` 是唯一判据。原实现长在 CPU 侧批行
                // 写入端，随「CPU 侧 per-batch 表构建」整族删除后迁到本处：
                // 本事实行是它的**新载体**（剔除端按 `entity.shadow_*` 抄进 per-draw
                // 行 ⇒ D3 契约段③ 由两跳承担）。
                // `receive_shadow` 事实仍由实体级自由函数提供（缺省约定唯一出处见 ShadowProxy.h）。
                StrategyFacts shadow_facts;
                shadow_facts.receive_shadow = CanReceiveShadow(shadow_owner);

                const uint32_t shadow_needs =
                    EvaluateRenderNeed(shadow_owner ? shadow_owner->BuildComponentMask() : 0u,
                                       shadow_facts.ToMask());

#if ULRE_STRATEGY_PARITY_ENABLED
                // 反向守卫：参考实现 = 旧判据（Release 下整块编空）
                ParityCheckShadowReceiver(
                    HasRenderNeed(shadow_needs,RenderNeed::ShadowReceiver),
                    CanReceiveShadow(shadow_owner),
                    "RenderCollectSystem#shadow-facts");
#endif

                world->GetEntityGPUTable().SetShadowFacts(
                    entity_id,
                    NormalizeShadowBiasToRow(GetShadowBiasMultiplier(shadow_owner)),
                    HasRenderNeed(shadow_needs,RenderNeed::ShadowReceiver) ? 1u : 0u);

                // ECS #4 **(b)（2026-10-14 落地）**：每帧按权威**重算 ②**（实体行 `Visible` 位）。
                // 为什么只刷这一处：这里正是"本项进批"判据点（4-ID 就在上面同一判据里发）⇒
                // 不变量「**4-ID 已发布 ⇒ ② == 权威（含祖先继承）**」成立，且剔除端能读到的行
                // 一律是当帧值。未进批的实体不刷：其 4-ID 已清零，剔除端 `geometry_id == 0`
                // 闸门**先于**任何昂贵计算（契约档 §5「问题 3 的 ②」裁定）。
                // 成本：值已在本帧别处算过同类查询（O(1)+上溯）；`SetVisible` 是**值变才标脏**
                // ⇒ 静态实体不产生脏行（否则整表每帧上传）。
                world->GetEntityGPUTable().SetVisible(entity_id, world->IsEntityVisible(entity_id));

                // ── T3.1 / T3.2：模型模板事实上行（GPU 展开分支的**唯一入口**）──
                // 同一判据点、同一纪律：本实体已进批 ⇒ 行值当帧有效。`template_id != 0`
                // 时剔除 shader 会按模板部件表逐部件展开（实体自己**不再**为每个部件发
                // item）；`0` ⇒ 传统单实体路径（掩码一并归零，避免残留掩码影响日后重绑）。
                // 成本：`SetModelFacts` 值变才标脏 ⇒ 稳态零脏行。
                //
                // 注意：这里**必须每帧无条件重发**，不能按"组件没变"跳过 ——
                // `GPUModelRefComponent::HidePart/ShowPart/ShowAllParts` 是裸写掩码字段，
                // **没有**任何 dirty 通知；一旦跳过，部件显隐就会停滞在上一帧的掩码上。
                {
                    const GPUModelRefComponent *model_ref =
                        shadow_owner ? shadow_owner->GetComponent<GPUModelRefComponent>() : nullptr;

                    world->GetEntityGPUTable().SetModelFacts(
                        entity_id,
                        model_ref ? model_ref->GetTemplateID() : 0u,
                        model_ref ? model_ref->part_visibility_mask : 0ull,
                        model_ref ? model_ref->custom_data_index : 0u,
                        model_ref ? model_ref->prev_transform_id : 0u);
                }

#if ULRE_ECS_DEBUG_API
                // 写后一致断言（debug 面）：这条**本该恒成立**（刚写的就是权威）⇒ 一旦点名即
                // 行缺失/溢出/有第二写者。取数：EntityGPUTable::GetVisibleMirrorCount()（应恒 0）。
                world->GetEntityGPUTable().VerifyVisibleMirror(
                    shadow_owner, world->IsEntityVisible(entity_id),
                    "RenderCollectSystem#publish");
#endif//ULRE_ECS_DEBUG_API
            }
        }

        if (!ids_published)
        {
            // 时点 2（几何行不成立 ⇒ 本项不进批）：**显式清零本 pass 那一半**。
            // 为什么：留着上一帧的值会让剔除按**旧几何的 AABB** 记账（画面不受影响 —— 该实体不在任何批里、
            // 被 tags 跳过 —— 但剔除账与计数会偏）；零 = 未上行，与 `EntityRenderIDTable::Clear()`
            //（实体销毁清两半）以及剔除侧 `geometry_id == 0 ⇒ 跳过本帧` 是同一约定。
            // 成本：表的 setter 值变才标脏 ⇒ 稳态只在"有 → 无"那一帧脏一次。
            world->GetEntityRenderIDTable().SetRenderIDs(pass_index, entity_id, graph::RenderItemDescriptor{});

#if ULRE_ECS_DEBUG_API
            const graph::RenderItemDescriptor *verif_row =
                world->GetEntityRenderIDTable().Row(pass_index, entity_id.index);
            if (verif_row && !(*verif_row == graph::RenderItemDescriptor{}))
            {
                GLogError("[EntityRenderIDTable] 几何行不成立但本 pass 行未清零：entity_index=%u pass=%u geometry_id=%u",
                          entity_id.index, pass_index, geometry_id);
            }
#endif
        }

        return ids_published;
    }
}//namespace hgl::ecs
