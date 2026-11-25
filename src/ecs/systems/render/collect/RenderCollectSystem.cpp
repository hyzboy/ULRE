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
        /// A2/A5b 查询：实体的**材质授权记录**（无实体/无世界/从未授权时 nullptr）。
        /// 2026-11：MaterialData 已退役为纯旁表记录（键 = EntityID）——渲染侧只**读**，
        /// 绝不再「无则创建」：从未改过材质的实体本就没有授权状态，零内存才是正解
        ///（原 `EnsureMaterialDataOf` 是压测场景行池/内存报错的根因）。
        MaterialData *FindMaterialDataOf(Entity *entity)
        {
            ECSContext *context = entity ? entity->GetContext() : nullptr;

            return context ? context->GetMaterialData(entity) : nullptr;
        }

        /// 材质授权代数（A2 之前挂在图元组件上，随授权状态一并迁入数据层）。
        uint32_t MaterialAuthoredGenerationOf(Entity *entity)
        {
            MaterialData *material_data = FindMaterialDataOf(entity);

            // 2026-11：代数真源 = 世界级旁表记录（`MaterialData::authored_generation`，
            // 键 = EntityID）。原「设备级组件行行内 revision」已随组件行池退役。
            return material_data ? material_data->GetAuthoredGeneration() : 0;
        }

        // ── A3/A4：材质变体表 + 材质运行期表的取用入口 ──
        // 变体表归世界所有（见 MaterialVariantTable.h 的"归属"）；program 本体在表记录里
        // （按静态键去重），运行期表只持**变体 ID** 与共享绑定。本文件所有 program 取用都
        // 经"运行期共享行 → 变体 ID → 变体记录"这条唯一链——不再有"组件上还有第二份
        // program 指针/绑定状态"的形态（原材质运行期组件已删）。
        MaterialVariantTable *GetVariantTable(ECSContext *world)
        {
            return world ? world->GetMaterialVariantTable() : nullptr;
        }

        MaterialVariantRecord *GetVariantRecordMutable(MaterialVariantTable *table,
                                                       const MaterialVariantID id)
        {
            return table ? table->GetMutable(id) : nullptr;
        }

        graph::ShaderProgram *GetForwardProgram(MaterialVariantTable *table,
                                                const MaterialRuntimeRow *row)
        {
            const MaterialVariantRecord *record = (table && row)
                ? table->Get(row->forward_variant)
                : nullptr;

            return record ? record->program : nullptr;
        }

        graph::ShaderProgram *GetShadowProgram(MaterialVariantTable *table,
                                               const MaterialRuntimeRow *row)
        {
            const MaterialVariantRecord *record = (table && row)
                ? table->Get(row->shadow_variant)
                : nullptr;

            return record ? record->program : nullptr;
        }

        /// 当前 pass 的 program（阴影 pass = ShadowDepth 变体，主帧 = 前向变体）。
        graph::ShaderProgram *GetCurrentPassProgram(MaterialVariantTable *table,
                                                    const MaterialRuntimeRow *row,
                                                    const bool shadow_pass)
        {
            return shadow_pass ? GetShadowProgram(table, row)
                               : GetForwardProgram(table, row);
        }

        /// 退掉某条变体记录的 program 引用（记录仍是同键实体共享；program 本体归
        /// ShaderProgramManager 缓存持有）。同键实体会走完整解析从缓存里重新拿到同一
        /// program——渲染结果不变，只是多一次解析。
        void ClearVariantProgram(MaterialVariantTable *table, const MaterialVariantID id)
        {
            if (MaterialVariantRecord *record = GetVariantRecordMutable(table, id))
                record->program = nullptr;
        }

        /// 前向槽的 program purpose。A4 起阴影 pass 也要用它还原**同一行键**
        /// （运行期共享行的 program 身份维度恒取前向 purpose，两个 pass 落在同一行上）。
        graph::mtl::ShaderProgramPurpose GetEffectiveForwardPurpose(Entity *primitive_comp)
        {
            const GeometryData *geometry = FindGeometryOf(primitive_comp);

            switch (geometry ? geometry->GetPrimitiveVariantPurpose()
                             : graph::PrimitiveVariantPurpose::Surface)
            {
            case graph::PrimitiveVariantPurpose::DepthOnly:
                return graph::mtl::ShaderProgramPurpose::DepthOnly;
            case graph::PrimitiveVariantPurpose::ShadowCaster:
                return graph::mtl::ShaderProgramPurpose::ShadowDepth;
            default:
                return graph::mtl::ShaderProgramPurpose::ForwardColor;
            }
        }

        /// 运行期行键的"自有材质 SSBO 行"维度（无有效绑定记 0）。
        /// ⚠ 必须入键：`HashMaterialRecipe` 不含 `material_ssbo_binding.data_index`
        /// （inc/hgl/mtl/MaterialRecipe.h:641-648），只按配方去重会让"同配方、不同
        /// 数据行"的实体共用一行（PBRSpheres 每球一行数据 ⇒ 整体串味）。
        uint32_t ResolveRowDataIndex(const graph::mtl::MaterialRecipe &recipe)
        {
            return recipe.material_ssbo_binding.IsValid()
                ? recipe.material_ssbo_binding.data_index
                : 0u;
        }

        // ── A4：共享行的取/换（intern + refcount）──
        // 把 slot 的行引用换成 `key` 对应的**共享行**：
        //   · 同键（新行号 == 旧行号）⇒ 抵消这次 Intern 的引用，引用数不变；
        //   · 换行 ⇒ 先释放旧行（归零则回收并退休其 GPU 绑定），再挂新行。
        MaterialRuntimeRowID AssignSlotRow(MaterialRuntimeTable *runtime_table,
                                           MaterialRuntimeSlot &slot,
                                           const MaterialRuntimeKey &key)
        {
            if (!runtime_table)
                return INVALID_MATERIAL_RUNTIME_ROW_ID;

            const MaterialRuntimeRowID id = runtime_table->Intern(key);

            if (id == INVALID_MATERIAL_RUNTIME_ROW_ID)
                return INVALID_MATERIAL_RUNTIME_ROW_ID;

            if (slot.row == id)
            {
                runtime_table->Release(id);
                return id;
            }

            if (slot.row != INVALID_MATERIAL_RUNTIME_ROW_ID)
                runtime_table->Release(slot.row);

            slot.row = id;
            return id;
        }

        /// 释放 slot 持有的行引用（行归零 ⇒ 回收 + 退休 GPU 绑定）。
        void ReleaseSlotRow(MaterialRuntimeTable *runtime_table, MaterialRuntimeSlot &slot)
        {
            if (!runtime_table || slot.row == INVALID_MATERIAL_RUNTIME_ROW_ID)
                return;

            runtime_table->Release(slot.row);
            slot.row = INVALID_MATERIAL_RUNTIME_ROW_ID;
        }

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

            auto *gc = world ? world->GetGraphicsContext() : nullptr;
            auto *pool = gc ? gc->GetGlobalSSBOBufferRegistry() : nullptr;
            auto *dev = world ? world->GetGPUDevice() : nullptr;

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

        // ── A7b：材质缺失 ⇒ 回退（错误）材质 ────────────────────────────
        // 用户定稿语义：「有几何但无材质来源」**不是剔除条件**，而是**材质错误** ——
        // 必须用保底材质渲染出来，并由**根颜色**标注错误种类。

        /// 回退（错误）材质的**种类判定**：判据同样来自策略表
        /// （`RenderNeed::FallbackMaterial` = 几何槽位 + 禁 `HasMaterialSource` 事实），
        /// 种类再经 mtl 侧分类表（`ClassifyMaterialErrorKind`）给出 —— 两级都是数据驱动表。
        /// 事实只作输入，不是判据本身。
        graph::mtl::MaterialErrorKind ResolveFallbackMaterialErrorKind(Entity *owner)
        {
            StrategyFacts facts;
            facts.has_material_source = HasAnyMaterialSource(owner);
            // 其余事实保持缺省（真）：FallbackMaterial 规则不引用它们（只会让别的需求位
            // 多出），本函数只读 FallbackMaterial 位 ⇒ 与全量事实求值结果一致。
            // 掩码不再是实体字段（A1(b)）：由行表现场拼（见 `Entity::BuildComponentMask`）。
            const uint32_t needs = EvaluateRenderNeed(
                owner ? owner->BuildComponentMask() : 0u, facts.ToMask());

            if (!HasRenderNeed(needs, RenderNeed::FallbackMaterial))
                return graph::mtl::MaterialErrorKind::None;

            return graph::mtl::ClassifyMaterialErrorKind(facts.has_material_source);
        }

        /// 回退（错误）材质的**根颜色宿主**：按 (注册表, 错误种类) 缓存行号。
        /// 种类是有界枚举 ⇒ 行数有界（每种错误一行），不会随实体/帧增长。
        struct FallbackMarkerRows
        {
            graph::GlobalSSBOBufferRegistry *registry = nullptr;
            uint32_t row[graph::mtl::MATERIAL_ERROR_KIND_COUNT];

            FallbackMarkerRows()
            {
                for (uint32_t i = 0; i < graph::mtl::MATERIAL_ERROR_KIND_COUNT; ++i)
                    row[i] = graph::ActiveRowPool::InvalidRowID;
            }
        };

        FallbackMarkerRows g_fallback_marker_rows;

        /// 取（或首次创建）某错误种类的根颜色行。行**不归还**：它必须活到本世界最后
        /// 一帧回退材质消失为止（与示例长期持有 accessor 同义）。缓存按注册表指针 +
        /// `IsActive` 双重校验 ⇒ 图形上下文重建（新注册表/新池）时会重新申请，绝不会
        /// 读到别人的行。
        uint32_t AcquireFallbackMarkerRow(graph::GlobalSSBOBufferRegistry *registry,
                                          const graph::mtl::MaterialErrorKind kind)
        {
            const uint32_t index = static_cast<uint32_t>(kind);

            if (!registry || index >= graph::mtl::MATERIAL_ERROR_KIND_COUNT)
                return graph::ActiveRowPool::InvalidRowID;

            if (g_fallback_marker_rows.registry != registry)
            {
                g_fallback_marker_rows = FallbackMarkerRows{};
                g_fallback_marker_rows.registry = registry;
            }

            const graph::GlobalSSBOType type = graph::GlobalSSBOType::EmissiveSurface;

            if (registry->IsActive(type, g_fallback_marker_rows.row[index]))
                return g_fallback_marker_rows.row[index];

            const uint32_t row = registry->Acquire(type);
            if (row == graph::ActiveRowPool::InvalidRowID)
                return graph::ActiveRowPool::InvalidRowID;

            graph::ssbo::EmissiveSurfaceRow marker{};
            const graph::mtl::FallbackMaterialRule &rule =
                graph::mtl::GetFallbackMaterialRule(kind);
            marker.color = Color4f(rule.marker_color[0], rule.marker_color[1],
                                   rule.marker_color[2], rule.marker_color[3]);

            if (!registry->Write(type, row, marker))
            {
                registry->ReleaseID(type, row);
                return graph::ActiveRowPool::InvalidRowID;
            }

            g_fallback_marker_rows.row[index] = row;
            return row;
        }

        /// 把**根颜色**挂到回退配方上：经**已有机制**（材质数据行 EmissiveSurface.color →
        /// `recipe.material_ssbo_binding`）下发 —— 与 `builtin/pure_color` 完全同一条路径，
        /// 不新建并行机制。无图形设备（单元测试路径）时回退配方依然成立，只是颜色不下发；
        /// **绝不因此剔除图元**。
        bool AttachFallbackMarkerColor(Entity *primitive_comp,
                                       graph::mtl::MaterialRecipe &out_recipe)
        {
            if (out_recipe.fallback_error_kind == graph::mtl::MaterialErrorKind::None)
                return true;

            ECSContext *context = primitive_comp ? primitive_comp->GetContext() : nullptr;
            graph::GraphicsContext *graphics = context ? context->GetGraphicsContext() : nullptr;

            if (!graphics)
            {
                auto *render_context = context ? context->GetRenderContext() : nullptr;
                graphics = render_context ? render_context->GetGraphicsContext() : nullptr;
            }

            auto *registry = graphics ? graphics->GetGlobalSSBOBufferRegistry() : nullptr;
            const uint32_t row = AcquireFallbackMarkerRow(registry,
                                                          out_recipe.fallback_error_kind);

            if (row == graph::ActiveRowPool::InvalidRowID)
            {
                // 标记色行拿不到 ⇒ **必须降级到零依赖的灰度棋盘格**：规则表里的
                // `ErrorChecker3D` 领域声明了 `require Resource MaterialData`，
                // 没有 binding 会被 ValidateMaterialRecipeForRuntime 判定失败并剔除图元
                // （退化成第一起回归的报错形态）。降级后配方不需要 payload 行，且切到
                // Checker3D 领域 ⇒ program 会按新领域重新解析（三次 BuildResolvedRecipe
                // 走同一条路 ⇒ 一致）。
                out_recipe.domain = graph::mtl::kMarkerlessFallbackMaterialDomain;
                out_recipe.recipe_name = "fallback_without_marker";
                out_recipe.material_ssbo_binding = graph::GlobalSSBOBinding{};

                GLogWarning(u8"[MaterialFallback] marker row unavailable (no graphics device or pool not ready); degrade to markerless domain %s: owner=%s",
                            graph::mtl::GetMaterialDomainName(out_recipe.domain), GetOwnerName(primitive_comp));
                return true;
            }

            const graph::GlobalSSBOType type = graph::GlobalSSBOType::EmissiveSurface;

            graph::GlobalSSBOBinding binding{};
            binding.ssbo_type  = type;
            binding.ssbo_id    = registry->GetPool(type)
                ? registry->GetPool(type)->GetSSBOId() : 0u;
            binding.data_index = row;

            out_recipe.material_ssbo_binding = binding;
            return true;
        }

        // A7b：回退（错误）材质配方合成次数（诊断；Update 每帧开头清零）。
        uint32_t g_fallback_recipe_builds = 0;

        /// 采集「几何级材质错误」所需事实：顶点格式是否存在、是否含 Normal / TexCoord。
        /// 无几何资产 ⇒ `has_geometry_format = false` ⇒ 判据一律不成立（宁可不报）。
        graph::mtl::MaterialGeometryFacts BuildMaterialGeometryFacts(
            Entity *primitive_comp)
        {
            graph::mtl::MaterialGeometryFacts facts{};

            graph::PrimitiveType primitive_type = graph::PrimitiveType::Triangles;
            const graph::GeometryVertexFormat *geometry_vertex_format = nullptr;
            GetPrimitiveProgramBuildInputs(primitive_comp, primitive_type,
                                           geometry_vertex_format);

            if (!geometry_vertex_format)
                return facts;

            facts.has_geometry_format = true;
            facts.has_vertex_normal =
                geometry_vertex_format->Find(graph::VertexSemantic::Normal) != nullptr;
            facts.has_texture_coord =
                geometry_vertex_format->Find(graph::VertexSemantic::TexCoord) != nullptr;

            return facts;
        }

        /// 取配方对应的材质定义：由领域契约 + 配方合成。失败（配方无有效领域）⇒
        /// 用回退领域合成，绝不剔除图元。
        bool ResolveResolvedMaterial(
            const graph::mtl::MaterialRecipe &recipe,
            graph::mtl::ResolvedMaterial &out_definition)
        {
            if (graph::mtl::ResolveMaterialFromRecipe(recipe, out_definition))
                return true;

            graph::mtl::MaterialRecipe fallback_recipe;
            if (!graph::mtl::BuildFallbackMaterialRecipe(
                    fallback_recipe, graph::mtl::MaterialErrorKind::MissingMaterialSource))
                return false;

            return graph::mtl::ResolveMaterialFromRecipe(
                fallback_recipe, out_definition);
        }

        /**
         * A7d：**几何级**材质错误 ⇒ 整条配方换成错误棋盘格。
         *
         * 与 A7b/A7c 的区别：那两类是**配方/定义写错**（数据层看得到），这一类是
         * **定义与配方都合法、错在几何顶点格式与材质需求对不上**（网格做错了 / 顶点
         * 格式选错了）—— 数据层拿不到 `geometry_vertex_format`，只有这里能判。
         *
         * 为什么必须回退而不是让 ShaderGen 静默降级（用户 2026-10-24 定稿）：
         *   · 缺顶点法线降级成面法线平面着色后，画面看起来「像正常的低模」，
         *     法线数据坏了这件事被完全掩掉；
         *   · 缺 UV 则按垃圾 UV 采样贴图，表现为随机花纹，比没贴图更难查。
         * 两者都要肉眼一眼可见 ⇒ 统一走错误棋盘格，颜色区分种类（`kFallbackMaterialRules`）。
         *
         * 失败语义：合成回退配方失败 ⇒ 保持原配方（宁可画得不对也不剔除图元）。
         */
        void ApplyGeometryErrorFallback(Entity *primitive_comp,
                                        graph::mtl::MaterialRecipe &out_recipe)
        {
            graph::mtl::ResolvedMaterial definition{};

            if (!ResolveResolvedMaterial(out_recipe, definition))
                return;

            const graph::mtl::MaterialErrorKind geometry_error =
                graph::mtl::ClassifyMaterialGeometryError(
                    definition, BuildMaterialGeometryFacts(primitive_comp));

            if (geometry_error == graph::mtl::MaterialErrorKind::None)
                return;

            const graph::mtl::FallbackMaterialRule &rule =
                graph::mtl::GetFallbackMaterialRule(geometry_error);

            GLogError(u8"[MaterialFallback] 材质 \"%s\" 与几何顶点格式不匹配（错误种类 %s）"
                      u8"=> 回退错误材质 %s（棋盘格，颜色=(%.2f,%.2f,%.2f,%.2f)）owner=%s",
                      out_recipe.recipe_name.c_str(),
                      rule.marker_name,
                      graph::mtl::GetMaterialDomainName(rule.domain),
                      rule.marker_color[0], rule.marker_color[1],
                      rule.marker_color[2], rule.marker_color[3],
                      GetOwnerName(primitive_comp));

            if (!graph::mtl::BuildFallbackMaterialRecipe(out_recipe, geometry_error))
            {
                GLogError(u8"[MaterialFallback] 错误材质合成失败（错误种类 %s）=> 保留原配方 owner=%s",
                          rule.marker_name,
                          GetOwnerName(primitive_comp));
            }
        }

        bool BuildResolvedRecipe(Entity *primitive_comp,
                                 const graph::ShaderProgram *material_program,
                                 graph::mtl::MaterialRecipe &out_recipe)
        {
            if (!primitive_comp)
                return false;

            // 授权记录只**读**：从未改过材质的实体走一条默认构造的局部空记录
            //（语义 = 无任何覆盖 ⇒ 解析只用 asset 默认配方），零堆分配。
            const MaterialData authored_fallback{};
            const MaterialData *material_data = FindMaterialDataOf(primitive_comp);

            if (!material_data)
                material_data = &authored_fallback;

            // asset 里的默认配方是基底，数据层里的配方覆盖是覆盖源（配方来源 = 实体的
            // GeometryData 组件；A5a 起由几何组件持有 asset）。
            const GeometryData *geometry = FindGeometryOf(primitive_comp);

            if (material_data->BuildResolvedRecipe(out_recipe,
                                                   material_program,
                                                   geometry ? geometry->GetAssetMaterialRecipe() : nullptr))
            {
                // A7d：几何级材质错误 —— 定义与配方都合法，错在顶点格式对不上需求
                //（缺 Normal / 缺 UV0）。必须在**成功路径**上也判：这正是「材质没写错、
                // 网格做错了」的场景，数据层与 ShaderGen 都不会报。
                if (out_recipe.fallback_error_kind == graph::mtl::MaterialErrorKind::None)
                    ApplyGeometryErrorFallback(primitive_comp, out_recipe);

                // A7b ②：构建"成功"，但 mtl 侧可能已把**无法解析的材质定义 ID** 换成保底材质
                // （`NormalizeRecipe` 的分类结果记在 `fallback_error_kind` 上），A7d 也可能刚
                // 换了配方 ⇒ 计入回退次数。
                // 报错由各自的判定点承担（见 MaterialResolver.cpp / 上面的
                // ApplyGeometryErrorFallback），这里**不重复出声**；该位同时是"这帧真的走了
                // 错误材质"的取数口（配合上面的每帧汇总行）。
                if (out_recipe.fallback_error_kind != graph::mtl::MaterialErrorKind::None)
                    ++g_fallback_recipe_builds;

                return AttachFallbackMarkerColor(primitive_comp, out_recipe);
            }

            // ── A7b：材质错误（用户定稿）**不是剔除条件** ──────────────────────
            // 「有几何但无材质来源」由策略表表达为 `RenderNeed::FallbackMaterial`
            // （几何槽位 + 禁 HasMaterialSource），种类由 mtl 侧分类表给出。这里按表结论
            // 合成**回退（错误）材质**，照常产出渲染项（根颜色标注错误种类）。
            // 有材质来源却构建失败（纹理名非法等）⇒ 种类为 None ⇒ 保持既有失败语义，
            // 回退不掩盖真错误。
            const graph::mtl::MaterialErrorKind fallback_kind =
                ResolveFallbackMaterialErrorKind(primitive_comp);

            if (fallback_kind == graph::mtl::MaterialErrorKind::None)
                return false;

            if (!graph::mtl::BuildFallbackMaterialRecipe(out_recipe, fallback_kind))
                return false;

            ++g_fallback_recipe_builds;

            const graph::mtl::FallbackMaterialRule &rule =
                graph::mtl::GetFallbackMaterialRule(fallback_kind);

            GLogWarning(u8"[MaterialFallback] material error (%s) => fallback material %s color=(%.2f,%.2f,%.2f,%.2f) owner=%s",
                        rule.marker_name,
                        out_recipe.recipe_name.c_str(),
                        rule.marker_color[0], rule.marker_color[1],
                        rule.marker_color[2], rule.marker_color[3],
                        GetOwnerName(primitive_comp));

            return AttachFallbackMarkerColor(primitive_comp, out_recipe);
        }

        bool ResolveGraphicsAndMaterialManager(
            ECSContext *world,
            graph::GraphicsContext *&out_graphics,
            graph::ShaderProgramManager *&out_material_manager)
        {
            out_graphics = world->GetGraphicsContext();
            if (!out_graphics)
            {
                auto *render_context = world->GetRenderContext();
                out_graphics = render_context
                                   ? render_context->GetGraphicsContext()
                                   : nullptr;
            }
            if (!out_graphics)
                return false;

            out_material_manager = out_graphics->GetMaterialManager();
            return out_material_manager != nullptr;
        }

        // ambient provider 模块（`ambient_light_provider` 槽）由**世界/环境层**决定：
        // `EnvironmentSystem::SetAmbientProvider` → `EnvironmentManager::Profile::ambient_module_name`
        // → `ECSContext::GetAmbientProviderName()`。材质不再声明它 —— 环境光的算法与
        // 参数块（`ambient_env`）必须由同一个所有者配对。
        //
        // 世界没配环境（名字为空）时**规范化为 `ambient_light_none`**，而不是把空串
        // 透传给材质解析器：空串会让 `SelectScenePassProfileRequest()` 跳过覆盖、
        // 落到模板默认值 —— 那等于"没配环境 ⇒ 用某个算法"，是隐式行为。这里显式
        // 声明"没有天光"，前向材质仍有天光项参与计算（恒零），不出现 `#if` 分支。
        const AnsiString &ResolveAmbientProviderModule(ECSContext *world)
        {
            static const AnsiString none(graph::mtl::kAmbientLightProviderNone);

            if (!world)
                return none;

            const AnsiString &name = world->GetAmbientProviderName();
            return name.IsEmpty() ? none : name;
        }

        // provider 名按“长度 + 字节内容”显式哈希：不依赖字符串类型到 `operator<<`
        // 的重载选择（隐式转换/字节表示差异会让不同名字算出同一个哈希）。
        uint64_t HashAmbientProviderModule(const AnsiString &name)
        {
            const char *text = name.c_str();
            const size_t length = text ? strlen(text) : 0;
            hgl::hash::FNV1aHasher64 hasher;
            hasher << static_cast<uint32>(length);
            if (length > 0)
                hasher.AppendBytes(text, length);
            return hasher;
        }

        graph::ShaderProgram *EnsureTemplateGroupProgram(
            ECSContext *world,
            MaterialVariantTable *variant_table,
            MaterialRuntimeTable *runtime_table,
            MaterialRuntimeRowID row_id,
            bool shadow_pass,
            graph::PrimitiveType primitive_type,
            const graph::GeometryVertexFormat *vertex_format)
        {
            if (!world || !variant_table || !runtime_table || row_id == INVALID_MATERIAL_RUNTIME_ROW_ID)
                return nullptr;

            MaterialRuntimeRow *row = runtime_table->GetMutable(row_id);
            if (!row)
                return nullptr;

            const graph::mtl::MaterialRecipe &effective_recipe = row->cached_effective_recipe;

            if (effective_recipe.recipe_name.empty())
                return nullptr;

            graph::GraphicsContext *graphics = nullptr;
            graph::ShaderProgramManager *material_manager = nullptr;
            if (!ResolveGraphicsAndMaterialManager(world, graphics, material_manager) || !material_manager)
                return nullptr;

            const AnsiString &ambient_provider = ResolveAmbientProviderModule(world);
            const auto purpose = shadow_pass ? graph::mtl::ShaderProgramPurpose::ShadowDepth
                                             : graph::mtl::ShaderProgramPurpose::ForwardColor;
            const auto build_hash = graph::mtl::HashMaterialProgramBuildContext(
                primitive_type, vertex_format,
                graphics->GetPhysicalDeviceProfile(), purpose, ambient_provider.c_str());
            const auto variant_id = variant_table->Intern(MaterialVariantKey{
                build_hash, graph::mtl::HashMaterialRecipe(effective_recipe)});
            auto *record = variant_table->GetMutable(variant_id);
            if (!record)
                return nullptr;
            if (shadow_pass)
            {
                row->shadow_variant = variant_id;
                row->shadow_cached_normalized_recipe = effective_recipe;
            }
            if (record->program)
                return record->program;

            graph::mtl::MaterialBuildRequest mtl_request{};
            mtl_request.recipe = effective_recipe;
            mtl_request.primitive_type = primitive_type;
            mtl_request.geometry_vertex_format = vertex_format;
            mtl_request.shader_program_purpose = purpose;
            mtl_request.ambient_module_name = ambient_provider.c_str();

            graph::mtl::ResolvedMaterial template_definition{};
            if (!ResolveResolvedMaterial(effective_recipe, template_definition))
                return nullptr;

            if (!graph::SelectCurrentScenePassProfileRequest(
                    template_definition, mtl_request,
                    mtl_request.render_template_request))
                return nullptr;

            graph::ShaderProgram *resolved_program =
                material_manager->AcquireShaderProgram(mtl_request);

            if (resolved_program)
            {
                record->program = resolved_program;
            }

            return resolved_program;
        }

        const graph::mtl::RecipeTextureBinding *FindRecipeTextureBinding(
            const graph::mtl::MaterialRecipe &recipe,
            const std::string &texture_name) noexcept
        {
            for (const auto &binding : recipe.textures)
            {
                if (binding.texture_name == texture_name)
                    return &binding;
            }
            return nullptr;
        }

        bool ValidateMaterialRecipeForRuntime(
            const graph::mtl::MaterialRecipe &recipe,
            const graph::ShaderProgram *material_program,
            const graph::mtl::ResolvedMaterial &definition,
            const char *owner_name)
        {
            if (!material_program)
                return false;

            for (size_t i = 0; i < recipe.textures.size(); ++i)
            {
                const auto &binding = recipe.textures[i];
                if (!graph::mtl::IsValidMaterialTextureName(
                        binding.texture_name))
                {
                    GLogError(
                        "[MaterialBinding] Invalid texture name owner=%s texture=%s",
                        owner_name ? owner_name : "<null>",
                        binding.texture_name.c_str());
                    return false;
                }

                if (binding.required && binding.resource_id.empty())
                {
                    GLogError(
                        "[MaterialBinding] Required texture resource missing owner=%s texture=%s",
                        owner_name ? owner_name : "<null>",
                        binding.texture_name.c_str());
                    return false;
                }

                for (size_t j = 0; j < i; ++j)
                {
                    if (recipe.textures[j].texture_name
                            == binding.texture_name)
                    {
                        GLogError(
                            "[MaterialBinding] Duplicate texture binding owner=%s texture=%s",
                            owner_name ? owner_name : "<null>",
                            binding.texture_name.c_str());
                        return false;
                    }
                }

                const int declaration_index =
                    graph::mtl::FindMaterialTextureDeclaration(
                        definition,
                        binding.texture_name);
                if (!definition.texture_declarations.empty()
                 && declaration_index < 0)
                {
                    GLogError(
                        "[MaterialBinding] Undeclared texture owner=%s texture=%s definition=%s",
                        owner_name ? owner_name : "<null>",
                        binding.texture_name.c_str(),
                        definition.domain_name.c_str());
                    return false;
                }

                if (declaration_index >= 0)
                {
                    const auto &declaration =
                        definition.texture_declarations[
                            static_cast<size_t>(declaration_index)];
                    if (binding.array_layer != 0
                     && !graph::mtl::IsMaterialTextureArraySampler(
                            declaration.sampler_type))
                    {
                        GLogError(
                            "[MaterialBinding] Non-array texture received array layer owner=%s texture=%s layer=%u",
                            owner_name ? owner_name : "<null>",
                            binding.texture_name.c_str(),
                            binding.array_layer);
                        return false;
                    }
                }
            }

            for (const auto &declaration : definition.texture_declarations)
            {
                const auto *binding = FindRecipeTextureBinding(
                    recipe,
                    declaration.name);
                if (!binding && declaration.required)
                {
                    GLogError(
                        "[MaterialBinding] Required texture binding missing owner=%s texture=%s",
                        owner_name ? owner_name : "<null>",
                        declaration.name.c_str());
                    return false;
                }

                if (binding
                 && (binding->required || declaration.required)
                 && binding->resource_id.empty())
                {
                    GLogError(
                        "[MaterialBinding] Required texture resource missing owner=%s texture=%s",
                        owner_name ? owner_name : "<null>",
                        declaration.name.c_str());
                    return false;
                }
            }

            // ── 材质私有数据 SSBO 行绑定 ────────────────────────────────────────
            // 程序**真的读 payload 行**（Fragment 里有 `MTL_ROW`）时，渲染准备阶段必须
            // 能拿到一行真实存在的 SSBO 行地址。
            //
            // 这里原本没有任何校验：绑定位无效时 MaterializeRecipeRowsForPrimitive 只会把
            // material_row_gpu 留在 0，然后把 BDA 地址 0 直接发给 shader —— 着色器不会
            // 报错也不会崩，只是从此读到「第一行」别人的材质数据（黑块/错色/随机闪烁），
            // 排查成本极高。所以在解析期就拒绝，让图元回退到错误材质，问题当场可见。
            //
            // 判据必须用 `RequiresRecipePayloadRow()` 而不是合并信号
            // `RequiresRecipeRuntimeRows()`：后者还包含「纹理引用行需求」
            // （`MTL_TEX` 的 `texture_reference_index`），那一类只经
            // `MaterialInstanceAddresses` 行表 + 4-ID 表的 `texture_id` 寻址，
            // 行号由 `material_texture_configuration.row_index` 独立取得，**从不读
            // payload_index**。纯纹理材质（`UnlitTexture` 等：无 payload、只用
            // `MTL_TEX`）如果按合并信号判定，就会被当成「缺绑定」错杀。
            if (material_program->RequiresRecipePayloadRow())
            {
                const auto &private_binding = recipe.material_ssbo_binding;
                if (!private_binding.IsValid())
                {
                    GLogError(
                        "[MaterialBinding] Program requires runtime rows but material SSBO binding is invalid owner=%s definition=%s ssbo_id=%u data_index=%u",
                        owner_name ? owner_name : "<null>",
                        definition.domain_name.c_str(),
                        private_binding.ssbo_id,
                        private_binding.data_index);
                    return false;
                }

                // ssbo_type 决定行的**布局**（字段宽度/顺序由该类型的结构版本给出）。
                // 类型错配会让 shader 按 A 结构读 B 结构的字节，同样不会报错，
                // 只会读到一堆看似合理的垃圾值。
                if (private_binding.ssbo_type
                        != definition.material_private_data)
                {
                    GLogError(
                        "[MaterialBinding] Material SSBO type mismatch owner=%s definition=%s expected=%s actual=%s",
                        owner_name ? owner_name : "<null>",
                        definition.domain_name.c_str(),
                        hgl::serialize::EnumNameOf(
                            definition.material_private_data),
                        hgl::serialize::EnumNameOf(
                            private_binding.ssbo_type));
                    return false;
                }
            }

            return true;
        }

        bool PrepareActivePlanResources(
            ECSContext *world,
            Entity *primitive_comp,
            graph::ShaderProgram *material_program,
            const graph::mtl::MaterialRecipe &active_recipe)
        {
            if (!world || !primitive_comp || !material_program)
                return false;

            graph::mtl::ResolvedMaterial definition{};
            if (!ResolveResolvedMaterial(active_recipe, definition)
             || !ValidateMaterialRecipeForRuntime(
                    active_recipe,
                    material_program,
                    definition,
                    GetOwnerName(primitive_comp)))
                return false;

            auto rdbs = world->GetSystem<RenderSceneUBOSystem>();
            auto *render_context = world->GetRenderContext();
            auto *graphics_context = render_context
                ? render_context->GetGraphicsContext()
                : world->GetGraphicsContext();
            auto *bindless_mgr = graphics_context
                ? graphics_context->
                    GetManager<graph::BindlessTextureManager>()
                : nullptr;
            if (!rdbs)
                return false;

            const char *owner_name =
                GetOwnerName(primitive_comp);
            const MaterialData *material_data =
                FindMaterialDataOf(primitive_comp);

            for (const auto &binding : active_recipe.textures)
            {
                if (binding.resource_id.empty())
                    continue;

                const auto *resource = material_data
                    ? material_data->GetTextureResource(binding.texture_name)
                    : nullptr;
                // 纹理来源两条，语义同一套：实体级 `SetTextureResource`（同名覆盖）优先，
                // 其次配方级 `RecipeTextureBinding::asset`（材质原型固有属性，多数场景只用这条）。
                // 未就绪的句柄（只有文件名 / RT 尚未出图）不该走到这里 —— `CanRender` 会先把图元
                // 拦下；走到这里说明就绪判定与收集判定脱节，直接报错更容易定位。
                // 采样器不参与判据：Bindless 预设（`contract/sampler.toml` → `samplers[]`）统一管理。
                graph::TextureAsset *binding_asset =
                    resource ? resource->asset : binding.asset;
                graph::Texture *resource_texture =
                    (binding_asset && binding_asset->IsReady())
                    ? binding_asset->GetTexture()
                    : nullptr;
                if (!binding_asset || !resource_texture || !bindless_mgr)
                {
                    // Step 5.2 占位保底：**永久失败**（`LoadFailed`，流送系统不自动重试）的纹理
                    // 不再让整条材质链失败 —— 该槽位保持 bindless 句柄 0，由 GLSL 侧
                    // `SampleOptional()` / `if (handle == 0u)` 的 fallback 兜底（白基色、未扰动法线）。
                    // 否则一个坏文件就会让整个图元永远进不了收集（静默消失）。
                    if (binding_asset
                     && resource_texture == nullptr
                     && binding_asset->GetPayloadState() == hgl::PayloadState::LoadFailed)
                    {
                        // 材质链成功后 `valid=true` 且代数不变 ⇒ 本分支每个图元最多走一次，
                        // 不会刷屏；坏文件本身由 `TextureAsset::FailAsyncLoad` 报过 Error。
                        GLogWarning(
                            "[DeferredResource] Texture load failed, slot falls back to handle 0: owner=%s texture=%s",
                            owner_name,
                            binding.texture_name.c_str());
                        continue;
                    }

                    // **仍在途**（`MetaOnly` / `Loading`）：流送通路的正常时序 —— 图元先入世界、
                    // 纹理后到。这时本帧只是"还不该画"，不是错误（`CanRender` 下一次就放行）。
                    // 降为 Debug：否则渐进装配期间每个图元每段代际刷两条红的，
                    // 真错误会被埋掉。
                    if (binding_asset && resource_texture == nullptr)
                    {
                        const hgl::PayloadState state =
                            binding_asset->GetPayloadState();
                        if (state == hgl::PayloadState::MetaOnly
                         || state == hgl::PayloadState::Loading)
                        {
                            GLogDebug(
                                "[DeferredResource] Texture still streaming, primitive waits: owner=%s texture=%s",
                                owner_name,
                                binding.texture_name.c_str());
                            return false;
                        }
                    }

                    GLogError(
                        "[DeferredResource] Texture acquisition failed: owner=%s texture=%s required=%d override=%d state=%d bindless=%d",
                        owner_name,
                        binding.texture_name.c_str(),
                        binding.required ? 1 : 0,
                        resource ? 1 : 0,
                        binding_asset
                            ? static_cast<int>(binding_asset->GetPayloadState())
                            : -1,
                        bindless_mgr ? 1 : 0);
                    return false;
                }

                const std::string resource_id =
                    BuildTextureResourceId(binding_asset);
                if (resource_id != binding.resource_id)
                {
                    GLogError(
                        "[DeferredResource] Texture identity mismatch: owner=%s texture=%s",
                        owner_name,
                        binding.texture_name.c_str());
                    return false;
                }

                // 所有纹理（2D / 2DArray）统一走 bindless Register。
                // resource->kind 仅保留用于资产加载（authoring）分支，
                // 描述符侧 2D 与 2DArray 均落在 sampler2DArray[]（单层/多层）。
                uint32_t handle = rdbs->RegisterTextureResource(
                    resource_id,
                    resource_texture,
                    bindless_mgr);
                if (handle == 0)
                    return false;
            }
            return true;
        }

        // A4：材质运行期失效。两种形态（都只动**本实例**的 slot 标志）：
        //   · reset_row_in_place = false（**行键要变**：配方 / 自有 SSBO 行变了）⇒ 释放本
        //     实例对旧共享行的引用（行归零 ⇒ 回收 + 退休其纹理配置池行）；下次解析会
        //     登记新键的行。旧行若仍被同键邻居引用则原样保留（它们没变，不该被牵连）。
        //   · reset_row_in_place = true（**行键不变**：program 对象换了 / 物化失败）⇒ 就地
        //     清掉共享行上的物化绑定并退休纹理配置 —— program 身份不在行键里，同键共享者
        //     取到的是同一个 program 对象，本帧同样会重跑整链，因此就地重置不会让邻居
        //     拿到半份状态。配方缓存**不动**（与旧 ClearMaterializationRows 同口径）。
        void InvalidateRecipeRuntime(ECSContext *world,
                                     MaterialRuntimeSlot &slot,
                                     const bool reset_row_in_place,
                                     const bool clear_program)
        {
            MaterialVariantTable *variant_table = GetVariantTable(world);
            MaterialRuntimeTable *runtime_table = GetRuntimeTable(world);

            MaterialRuntimeRow *row = runtime_table
                ? runtime_table->GetMutable(slot.row)
                : nullptr;
            const MaterialVariantID forward_variant = row
                ? row->forward_variant
                : INVALID_MATERIAL_VARIANT_ID;

            if (row && reset_row_in_place)
            {
                runtime_table->RetireRowTextureConfiguration(*row);
                row->data_index_row = uint32_t(-1);
                row->material_row_gpu = 0;
                row->material_texture_row_gpu = 0;
                row->material_texture_zero_row_gpu = 0;
                row->material_texture_configuration_hash = 0;
            }
            else
            {
                ReleaseSlotRow(runtime_table, slot);
            }

            slot.runtime_dirty = true;
            slot.valid = false;

            if (clear_program)
            {
                // A3：退掉该键变体记录的 program 引用（同键实体共享该记录，它们下帧
                // 会从 ShaderProgramManager 缓存重新拿到同一 program）。
                ClearVariantProgram(variant_table, forward_variant);
                slot.program_dirty = true;
            }
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

            return ref && ref->template_id != graph::ssbo::kInvalidModelTemplateID;
        }

        /// 实体 + 世界 ⇒ 收集策略（表是唯一判据）
        CollectStrategy EvaluateCollectStrategy(ECSContext *world,const Entity *owner)
        {
            CollectStrategy out;

            const bool template_carrier = IsModelTemplateCarrier(owner);

            out.facts.entity_visible      = owner ? world->IsEntityVisible(owner->GetEntityID()) : false;
            out.facts.has_owner           = (owner != nullptr);
            out.facts.renderable          = template_carrier || CanRender(owner);
            out.facts.has_material_source = template_carrier || HasAnyMaterialSource(owner);
            out.facts.cast_shadow         = CanCastShadow(owner);
            out.facts.receive_shadow      = CanReceiveShadow(owner);
            out.facts.shadow_pass         = world->IsCurrentPassShadow();

            if (out.facts.shadow_pass && owner)
            {
                const float max_dist = GetShadowMaxDistance(owner);

                if (max_dist > 0.0f && world->HasShadowOrigin())
                {
                    const TransformAccessor transform =
                        world->GetTransformByEntity(owner->GetEntityID());

                    if (transform.IsValid())
                    {
                        const glm::vec3 diff =
                            transform.GetWorldPosition() - world->GetShadowOrigin();

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

    // A1：阴影 pass 的专用 ShadowCaster 程序解析（独立于下方 forward 链）。
    //
    // 前向着色程序画深度图会把整套 PBR/PCF/天空片元开销花在不存在的颜色附
    // 件上，且其 PCF 采样恰命中当前 pass 的 depth attachment（Vulkan
    // attachment feedback loop）。ShadowCaster 模板无 material/sky
    // descriptors，本路径只解析 program 与 CreatePipeline 消费的 normalized
    // recipe；物化行、纹理配置等 forward 槽状态一概不动——否则 purpose 每帧
    // Forward↔Shadow 乒乓会让 InvalidateRecipeRuntime 反复 retire 纹理配置，
    // 并禁用 P1-1 全干净帧快路径。
    bool RenderCollectSystem::EnsureShadowCasterProgram(Entity *primitive_comp,
                                                                  MaterialRuntimeSlot &slot)
    {
        if (!world || !primitive_comp)
            return false;

        MaterialRuntimeTable *runtime_table = GetRuntimeTable(world);

        graph::PrimitiveType primitive_type = graph::PrimitiveType::Triangles;
        const graph::GeometryVertexFormat *geometry_vertex_format = nullptr;
        GetPrimitiveProgramBuildInputs(primitive_comp, primitive_type,
                                       geometry_vertex_format);

        graph::GraphicsContext *graphics = nullptr;
        graph::ShaderProgramManager *material_manager = nullptr;
        if (!ResolveGraphicsAndMaterialManager(world, graphics, material_manager))
        {
            GLogWarning("[RenderCollectSystem] ShadowCaster resolve failed: graphics/material manager null for %s",
                        GetOwnerName(primitive_comp));
            return false;
        }

        // 与 forward 槽同一判据粒度：build context hash 覆盖 primitive_type/
        // 顶点格式/设备 profile/purpose——SetPrimitiveAsset、变体切换都不 bump
        // authored generation，只看 generation 会漏掉顶点格式变化；authored
        // generation 再覆盖 recipe/纹理 authored 变化。
        const uint64_t build_context_hash =
            graph::mtl::HashMaterialProgramBuildContext(
                primitive_type,
                geometry_vertex_format,
                graphics->GetPhysicalDeviceProfile(),
                graph::mtl::ShaderProgramPurpose::ShadowDepth);

        // A3 快路径：上次登记的阴影变体键两维都未变、program 已就绪、材质授权代
        // 未推进 ⇒ 本帧无事可做。旧实现先 Intern 再查记录（键一变自然落到新记录），
        // 这里改为先比键、键不变才认这条记录——省掉一次无谓的 recipe 构造。
        // A4：阴影变体 ID 挂在**运行期共享行**上（无行 ⇒ 无变体 ID ⇒ 走完整路径）。
        MaterialVariantTable *variant_table = GetVariantTable(world);
        if (variant_table)
        {
            const MaterialRuntimeRow *prev_row = runtime_table
                ? runtime_table->Get(slot.row)
                : nullptr;
            const MaterialVariantRecord *prev = variant_table->Get(
                prev_row ? prev_row->shadow_variant : INVALID_MATERIAL_VARIANT_ID);

            if (prev
             && prev->key.program_build_context == build_context_hash
             && prev->program
             && slot.shadow_tracked_material_data_generation
                    == MaterialAuthoredGenerationOf(primitive_comp))
                return true;
        }

        // 变体键 = (build context, **recipe 身份**)。第二维决定 program 身份 ⇒ 必须
        // 先于 Intern 取得：阴影 effective recipe 与 forward 那条链**同源**（都是
        // BuildResolvedRecipe(primitive_comp, nullptr, …)），这里照常构造一次取哈希。
        // 时序取舍（Intern 从"recipe 之前"下移到"recipe 之后、acquire 之前"）：
        //   · 模板选择 / AcquireShaderProgram 失败仍在 Intern 之后 ⇒ D9 的失败归属
        //     （首帧告警 + 超上限降频）**不变**（计数本体 A4 起在每实例 slot）；
        //   · 只有 BuildResolvedRecipe 自身失败这一窄路径由"已登记"变为"未登记"：
        //     AdvanceShadowRetry 无变体键可记账 ⇒ 不 bump（与 graphics 缺失同类，
        //     该失败本身逐帧有独立告警）。代价换来"不同材质不再共用一条记录"。
        graph::mtl::MaterialRecipe effective_recipe{};
        if (!BuildResolvedRecipe(primitive_comp, nullptr, effective_recipe))
        {
            GLogWarning("[RenderCollectSystem] ShadowCaster BuildResolvedRecipe failed for %s",
                        GetOwnerName(primitive_comp));
            return false;
        }

        const uint64_t recipe_hash = graph::mtl::HashMaterialRecipe(effective_recipe);

        // A4：先把**运行期共享行**挂好——键里的 program 身份取**前向** purpose，
        // 于是"阴影 pass 先于主帧"（首帧 prepass / 只有阴影帧的条带重画）也会落在与
        // forward 链**同一行**上，不会一物两行。阴影槽的共享绑定
        // （shadow_variant / shadow_cached_normalized_recipe）随行存放。
        if (runtime_table)
        {
            // 行键取**前向** purpose，ambient provider 也必须同口径折入：
            // 否则世界换 ambient 模块后行键不变，阴影侧会认回旧行。
            const uint64_t forward_build_context =
                graph::mtl::HashMaterialProgramBuildContext(
                    primitive_type,
                    geometry_vertex_format,
                    graphics->GetPhysicalDeviceProfile(),
                    GetEffectiveForwardPurpose(primitive_comp),
                    ResolveAmbientProviderModule(world).c_str());

            AssignSlotRow(runtime_table, slot,
                          MaterialRuntimeKey{forward_build_context, recipe_hash,
                                             ResolveRowDataIndex(effective_recipe)});
        }

        // ⚠ 本处**不走** `VariantKeyOf()`：阴影变体键用的是 **ShadowDepth** 的 build context
        // （上一段 `HashMaterialProgramPurpose::ShadowDepth` 那处），而上面的运行期行键用的是
        // **forward** 的 build context ⇒ 阴影变体键是 forward 行键的**兄弟键**（同 recipe、
        // 不同 purpose），不是它的前缀（见 `MaterialRuntimeTable.h` 的键空间权威链）。
        const MaterialVariantID shadow_variant = variant_table
            ? variant_table->Intern(MaterialVariantKey{build_context_hash, recipe_hash})
            : INVALID_MATERIAL_VARIANT_ID;

        if (shadow_variant == INVALID_MATERIAL_VARIANT_ID)
        {
            GLogWarning("[RenderCollectSystem] ShadowCaster variant unavailable/full for %s build_context=%llu recipe=%llu",
                        GetOwnerName(primitive_comp),
                        (unsigned long long)build_context_hash,
                        (unsigned long long)recipe_hash);
            return false;
        }

        // Intern 可能让 records 扩容搬家 ⇒ 重新取行/记录，不复用上面的指针。
        if (MaterialRuntimeRow *row = runtime_table ? runtime_table->GetMutable(slot.row) : nullptr)
            row->shadow_variant = shadow_variant;

        // 同键记录可能已被同材质/同上下文的其它实体解析过 ⇒ program 已就绪即复用。
        // 此处**不写** program（program 只在解析成功那一刻写一次，不能每帧写共享记录）。
        MaterialVariantRecord *shadow_record = variant_table->GetMutable(shadow_variant);
        if (shadow_record
         && shadow_record->program
         && slot.shadow_tracked_material_data_generation
                == MaterialAuthoredGenerationOf(primitive_comp))
            return true;

        // purpose 固定为 ShadowDepth：SelectCurrentScenePassProfileRequest
        // 依此分派 ShadowCasterOpaque/Masked（按 recipe 的 alpha_test）。
        graph::mtl::MaterialBuildRequest mtl_request{};
        mtl_request.recipe = effective_recipe;
        mtl_request.primitive_type = primitive_type;
        mtl_request.geometry_vertex_format = geometry_vertex_format;
        mtl_request.shader_program_purpose =
            graph::mtl::ShaderProgramPurpose::ShadowDepth;
        graph::mtl::ResolvedMaterial template_definition{};
        if (!ResolveResolvedMaterial(effective_recipe, template_definition))
        {
            GLogWarning(
                "[RenderCollectSystem] ShadowCaster cannot select template for material=%s",
                effective_recipe.recipe_name.c_str());
            return false;
        }
        if (!graph::SelectCurrentScenePassProfileRequest(
                template_definition, mtl_request,
                mtl_request.render_template_request))
        {
            GLogWarning(
                "[RenderCollectSystem] ShadowCaster template selection failed for material=%s",
                effective_recipe.recipe_name.c_str());
            return false;
        }

        graph::ShaderProgram *resolved_program =
            material_manager->AcquireShaderProgram(mtl_request);
        if (!resolved_program)
        {
            GLogWarning(
                "[RenderCollectSystem] ShadowCaster AcquireShaderProgram failed for %s recipe=%s",
                GetOwnerName(primitive_comp),
                effective_recipe.recipe_name.c_str());
            return false;
        }

        // effective_recipe 出自 MaterialData::BuildResolvedRecipe（数据层边界
        // 已 NormalizeRecipe），直接作 normalized recipe 供 CreatePipeline 用。
        // A3：解析成功 ⇒ 落 program 到该变体记录（同键实体共享；这是记录 program
        // 的唯一写入点之一，只在解析路径发生）。
        shadow_record->program = resolved_program;
        if (MaterialRuntimeRow *row = runtime_table ? runtime_table->GetMutable(slot.row) : nullptr)
            row->shadow_cached_normalized_recipe = effective_recipe;
        slot.shadow_tracked_material_data_generation =
            MaterialAuthoredGenerationOf(primitive_comp);
        return true;
    }

    bool RenderCollectSystem::EnsureMaterialProgramForPrimitive(Entity *primitive_comp,
                                                                          MaterialRuntimeSlot &slot)
    {
        if (!world || !primitive_comp)
            return false;

        // A1：阴影 pass 一律走专用 ShadowCaster 程序槽；下方解析链只服务
        // forward pass，不感知当前 pass。
        if (world->IsCurrentPassShadow())
            return EnsureShadowCasterProgram(primitive_comp, slot);

        return EnsureForwardProgram(primitive_comp, slot);
    }

    // Forward 槽解析（主帧着色程序）。阴影 pass 中 masked caster 的行物化
    // 也会借道此处（见主循环 A1-4 分支）——纹理行是 per-primitive 共享
    // 状态，与 program 无关。
    bool RenderCollectSystem::EnsureForwardProgram(Entity *primitive_comp,
                                                             MaterialRuntimeSlot &slot)
    {
        if (!world || !primitive_comp)
            return false;

        MaterialVariantTable *variant_table = GetVariantTable(world);
        MaterialRuntimeTable *runtime_table = GetRuntimeTable(world);
        if (!variant_table || !runtime_table)
            return false;

        // A3/A4：本实例的 forward 变体记录视图（变体 ID 挂在**运行期共享行**上，
        // 解析/比对/取用都以此为准）。
        // 注意：该指针在下一次 Intern 之前有效——下面写 program 前会重新取记录。
        MaterialRuntimeRow *row = runtime_table->GetMutable(slot.row);
        const MaterialVariantRecord *forward_record = variant_table->Get(
            row ? row->forward_variant : INVALID_MATERIAL_VARIANT_ID);

        // P3: Fast-path — if nothing has changed since last resolve, skip all work.
        if (!slot.program_dirty
            && forward_record
            && forward_record->program
            && slot.tracked_material_data_generation
                == MaterialAuthoredGenerationOf(primitive_comp))
            return true;

        graph::mtl::MaterialRecipe effective_recipe{};
        if (!BuildResolvedRecipe(primitive_comp, nullptr, effective_recipe))
        {
            GLogWarning("[RenderCollectSystem] BuildResolvedRecipe failed for %s",
                        GetOwnerName(primitive_comp));
            return false;
        }

        const uint64_t recipe_hash = graph::mtl::HashMaterialRecipe(effective_recipe);
        graph::GraphicsContext *graphics = nullptr;
        graph::ShaderProgramManager *material_manager = nullptr;
        if (!ResolveGraphicsAndMaterialManager(world, graphics, material_manager))
        {
            GLogWarning("[RenderCollectSystem] ResolveMaterialProgram failed: graphics/material manager null for %s",
                        GetOwnerName(primitive_comp));
            return false;
        }

        graph::PrimitiveType primitive_type = graph::PrimitiveType::Triangles;
        const graph::GeometryVertexFormat *geometry_vertex_format = nullptr;
        GetPrimitiveProgramBuildInputs(primitive_comp, primitive_type,
                                       geometry_vertex_format);

        // 渲染变体 purpose 必须先于脏检查解析——若 Forward↔Shadow 切换而
        // recipe/geometry/profile 不变，哈希不含 purpose 会复用错误的 program
        const graph::mtl::ShaderProgramPurpose effective_purpose =
            GetEffectiveForwardPurpose(primitive_comp);

        // 环境层选定的 ambient provider 是本路径 program 身份的一部分：
        // 换 ambient 模块 ⇒ 换 fragment 源码 ⇒ 必须换 program/变体。
        const AnsiString &ambient_provider = ResolveAmbientProviderModule(world);

        const uint64_t build_context_hash =
            graph::mtl::HashMaterialProgramBuildContext(
                primitive_type,
                geometry_vertex_format,
                graphics->GetPhysicalDeviceProfile(),
                effective_purpose,
                ambient_provider.c_str());

        // A4：行键 = (program 身份, 配方身份, **自有材质数据行**)。任一维变了 ⇒ 旧共享行
        // 不再代表本实例的授权态：释放本实例对旧行的引用（行归零则回收并退休其纹理配置
        // 池行；仍被同键邻居引用的旧行原样保留，不该被牵连）。
        if (!row
         || row->key.recipe_hash != recipe_hash
         || row->key.program_build_context != build_context_hash
         || row->key.data_index_row != ResolveRowDataIndex(effective_recipe))
        {
            slot.program_dirty = true;
            InvalidateRecipeRuntime(world, slot, false, false);
            row = nullptr;

            // 管线由 (program 身份, 规范化 recipe) 共同决定：配方内容变化后，
            // 即便 program 身份不变（例如仅 cull_mode 等渲染状态变化）也必须重建
            // 管线。原失效点在组件级配方设置里，A2 随
            // 配方覆盖迁到数据层后，改由这里在"配方内容确实变了"时触发。
            world->InvalidateEntityRuntimePipeline(primitive_comp->GetEntityID());
        }

        if (!slot.program_dirty
         && forward_record
         && forward_record->program)
        {
            // Generation may have advanced without changing the recipe/program
            // content (e.g. an author swapped a texture or data object but kept
            // the same resource id). Refresh the effective recipe as well, so
            // an instance-only data_index change reaches BDA materialization.
            if (slot.tracked_material_data_generation
                != MaterialAuthoredGenerationOf(primitive_comp))
            {
                // 行键没变 ⇒ 同一份授权态的载体；内容相同，写回是幂等的。
                if (MaterialRuntimeRow *refresh = runtime_table->GetMutable(slot.row))
                {
                    refresh->cached_effective_recipe = effective_recipe;
                    refresh->cached_effective_recipe_hash =
                        graph::mtl::HashMaterialRecipe(effective_recipe);
                }
                slot.runtime_dirty = true;
                slot.tracked_material_data_generation =
                    MaterialAuthoredGenerationOf(primitive_comp);
            }
            return true;
        }

        // SceneGraph owns the existing provider-shape routing policy. It
        // selects a concrete template request before ShaderGen is invoked.
        graph::mtl::MaterialBuildRequest mtl_request{};
        mtl_request.recipe = effective_recipe;
        mtl_request.primitive_type = primitive_type;
        mtl_request.geometry_vertex_format = geometry_vertex_format;
        mtl_request.shader_program_purpose = effective_purpose;
        // 环境层（世界）决定用哪个 ambient provider；材质只声明表面。
        mtl_request.ambient_module_name = ambient_provider.c_str();
        graph::mtl::ResolvedMaterial template_definition{};
        if (!ResolveResolvedMaterial(effective_recipe, template_definition))
        {
            GLogWarning(
                "[RenderCollectSystem] Cannot select template for material=%s",
                effective_recipe.recipe_name.c_str());
            return false;
        }
        if (!graph::SelectCurrentScenePassProfileRequest(
                template_definition, mtl_request,
                mtl_request.render_template_request))
        {
            GLogWarning(
                "[RenderCollectSystem] Template selection failed for material=%s",
                effective_recipe.recipe_name.c_str());
            return false;
        }
        graph::ShaderProgram *resolved_program =
            material_manager->AcquireShaderProgram(mtl_request);

        if (!resolved_program)
        {
            GLogWarning("[RenderCollectSystem] AcquireShaderProgram failed for %s recipe=%s",
                        GetOwnerName(primitive_comp),
                        effective_recipe.recipe_name.c_str());
            return false;
        }

        graph::mtl::MaterialRecipe material_binding_recipe{};
        if (!BuildResolvedRecipe(
                primitive_comp,
                resolved_program,
                material_binding_recipe)
         || !ValidateMaterialRecipeForRuntime(
                material_binding_recipe,
                resolved_program,
                template_definition,
                GetOwnerName(primitive_comp)))
        {
            GLogWarning(
                "[RenderCollectSystem] Direct material recipe validation failed for %s",
                GetOwnerName(primitive_comp));
            return false;
        }

        // A4：program 对象变了（旧记录里的 program 与新解析的不是同一个）⇒ 就地把本行的
        // 物化绑定作废并退休纹理配置。program 身份**不在行键里**，但同键共享者取到的是
        // 同一个 program 对象（缓存按请求去重）⇒ 它们本帧同样会重跑整链，就地重置不会
        // 让邻居拿到半份状态。
        const bool program_changed =
            !(forward_record && forward_record->program == resolved_program);
        if (program_changed)
        {
            InvalidateRecipeRuntime(world, slot, true, false);
            row = nullptr;
        }

        // A4：登记/复用**运行期共享行**（intern + refcount；同键多实体共用一行）。
        // 键 = (program 身份, 配方身份, 自有材质数据行)；`AssignSlotRow` 负责换行时的
        // 旧行释放（归零回收 + 退休 GPU 绑定）与"同键复用抵消这次引用"。
        // 注意：Intern 可能让行/记录两个容器扩容搬家 ⇒ 之后一律重新取指针。
        const MaterialRuntimeKey runtime_key{
            build_context_hash,
            recipe_hash,
            ResolveRowDataIndex(material_binding_recipe)};

        // A3：登记/复用该键的变体记录并落 program（记录 program 只在解析路径写）。
        // 变体键 = 运行期键的**前缀**（去掉"自有 SSBO 行"维）⇒ 这里由 `VariantKeyOf()`
        // **派生**而不是手抄两维：前缀关系由构造保证，日后给配方身份加维度只需改
        // `MaterialRuntimeKey`（`VariantKeyOf` 与编译期 static_assert 一起盯着）。
        // 该前缀的语义仍是：同 build context 下**不同材质必须落不同记录**，
        // 否则后解析者的 program 会覆盖先解析者（键少一维的 bug）。
        const MaterialVariantID forward_variant =
            variant_table->Intern(VariantKeyOf(runtime_key));
        if (forward_variant == INVALID_MATERIAL_VARIANT_ID)
        {
            GLogWarning("[RenderCollectSystem] forward variant unavailable/full for %s build_context=%llu recipe=%llu",
                        GetOwnerName(primitive_comp),
                        (unsigned long long)build_context_hash,
                        (unsigned long long)recipe_hash);
            return false;
        }

        // A4：登记/复用**运行期共享行**（intern + refcount；同键多实体共用一行）。
        // 键 = (program 身份, 配方身份, 自有材质数据行)；`AssignSlotRow` 负责换行时的
        // 旧行释放（归零回收 + 退休 GPU 绑定）与"同键复用抵消这次引用"。
        // 注意：Intern 可能让行/记录两个容器扩容搬家 ⇒ 之后一律重新取指针。
        const MaterialRuntimeRowID row_id = AssignSlotRow(runtime_table, slot, runtime_key);
        if (row_id == INVALID_MATERIAL_RUNTIME_ROW_ID)
        {
            GLogWarning("[RenderCollectSystem] runtime row unavailable/full for %s build_context=%llu recipe=%llu data_index=%u",
                        GetOwnerName(primitive_comp),
                        (unsigned long long)runtime_key.program_build_context,
                        (unsigned long long)runtime_key.recipe_hash,
                        runtime_key.data_index_row);
            return false;
        }

        row = runtime_table->GetMutable(row_id);
        row->forward_variant = forward_variant;
        if (MaterialVariantRecord *forward_written = variant_table->GetMutable(forward_variant))
            forward_written->program = resolved_program;

        {
            uint32_t planned_textures = 0;
            for (const auto &binding : material_binding_recipe.textures)
                if (!binding.resource_id.empty())
                    ++planned_textures;
            const uint32_t planned_data =
                material_binding_recipe.material_ssbo_binding.IsValid()
                    ? 1u : 0u;
            GLogVerbose(
                "[DeferredResource] owner=%s program=%s planned_texture=%u planned_data=%u recipe_texture=%zu recipe_data=%zu",
                GetOwnerName(primitive_comp),
                resolved_program->GetName().c_str(),
                planned_textures,
                planned_data,
                material_binding_recipe.textures.size(),
                planned_data);
        }
        slot.program_dirty = false;
        slot.valid = false;                     // 等价于原 MarkProgramResolved()
        row->recipe_hash = recipe_hash;
        // 问题 8(b)：共享行的行字段必须与身份键同值（自有行 key 为空 ⇒ 不检查）。台帐宏门住。
        MaterialRuntimeTable::VerifyWrittenRecipeHash(*row, recipe_hash, "RenderCollectSystem: 解析后写 recipe_hash");

        // effective_recipe 出自 MaterialData::BuildResolvedRecipe（数据层边界
        // 已 NormalizeRecipe），直接缓存供 CreatePipeline 使用，无需再规范化。
        row->cached_normalized_recipe = effective_recipe;

        // P3: Cache effective recipe (with program-resolved SSBO types) for
        // direct resource preparation and BDA materialization.
        row->cached_effective_recipe = material_binding_recipe;
        row->cached_effective_recipe_hash =
            graph::mtl::HashMaterialRecipe(material_binding_recipe);

        slot.tracked_material_data_generation = MaterialAuthoredGenerationOf(primitive_comp);

        return true;
    }

    bool RenderCollectSystem::ResolveRuntimePipelineForPrimitive(Entity *primitive_comp,
                                                                          MaterialRuntimeSlot &slot)
    {
        if (!world || !primitive_comp)
            return false;

        const MaterialRuntimeTable *runtime_table = GetRuntimeTable(world);
        const MaterialRuntimeRow *row = runtime_table ? runtime_table->Get(slot.row) : nullptr;
        if (!row)
            return false;

        // A1：阴影 pass 用 ShadowCaster 槽的 program/recipe。运行期管线按 render_pass
        // 键控缓存在**每实例 slot** 上（A5b：渲染组件已删），两槽各自对应不同
        // RenderPass，互不驱逐。
        const bool shadow_pass = world->IsCurrentPassShadow();
        graph::ShaderProgram *program = GetCurrentPassProgram(
            GetVariantTable(world), row, shadow_pass);
        if (!program)
            return false;

        // 当前渲染目标唯一权威：world->GetRenderTarget()（RenderContext 副本已删除；
        // RenderTo 切 RT 时会同步本世界的 render_target 指针）
        auto *render_target = world->GetRenderTarget();
        auto *render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        if (!render_pass)
            return false;

        // 每个 RenderPass 各自有解析好的管线（跨 RT 不互相驱逐）；
        // 复用校验含 program 身份——shader 更新（新 program 对象）时重建，
        // 防止旧 program 的 pipeline 被无限复用（masked 阴影失效根因）。
        // A5b：不在组件/slot 上另做 program 身份缓存（D2：指针身份会因对象地址复用误判）——
        // 管线复用交给 RenderPass::CreatePipeline 内部（按 shader 内容 hash 键控）。

        // ShadowCaster 无绑定 recipe，直接用解析槽里缓存的 normalized
        // recipe；forward 保持 effective/normalized 复用逻辑。
        graph::mtl::MaterialRecipe effective_recipe =
            shadow_pass
                ? row->shadow_cached_normalized_recipe
                : row->cached_effective_recipe;

        if (!shadow_pass
         && row->recipe_hash
                == row->cached_effective_recipe_hash)
            effective_recipe = row->cached_normalized_recipe;

        graph::mtl::MaterialPipelineConfig resolved_pipeline_config{};
        graph::Pipeline *resolved_pipeline = render_pass->CreatePipeline(program,
                                                                         effective_recipe,
                                                                         &resolved_pipeline_config);
        if (!resolved_pipeline)
        {
            GLogWarning("[RenderCollectSystem] ResolveRuntimePipeline failed: CreatePipeline failed for %s material=%s",
                        GetOwnerName(primitive_comp),
                        program->GetName().c_str());
            return false;
        }

        // B5-b：管线缓存落世界级旁表（键 = 本实体；slot 里不放指针 ⇒ 行宽才能冻结）
        // EDS 1/2/3：管线本身不带渲染状态 ⇒ 同一份旁表里连同**解析出的动态渲染状态**
        // 一起存（两都按 pass 键控、同源解析）；批装配时按同一个 pass 取出，进合批键。
        world->GetMaterialRuntimePipelineTable().Set(primitive_comp->GetEntityID(),
                                                     render_pass,
                                                     resolved_pipeline,
                                                     resolved_pipeline_config);
        return true;
    }

    bool RenderCollectSystem::MaterializeRecipeRowsForPrimitive(Entity *primitive_comp,
                                                                         MaterialRuntimeSlot &slot)
    {
        if (!world || !primitive_comp)
            return false;

        // A4：物化写的是**共享行**（同键多实体共用一份绑定）；每实例标志仍写 slot。
        MaterialRuntimeTable *runtime_table = GetRuntimeTable(world);
        MaterialRuntimeRow *row = runtime_table ? runtime_table->GetMutable(slot.row) : nullptr;

        if (!row)
            return false;

        graph::ShaderProgram *material_program =
            GetForwardProgram(GetVariantTable(world), row);

        if (!material_program
         || !material_program->RequiresRecipeRuntimeRows())
        {
            row->data_index_row = 0;
            slot.runtime_dirty = false;
            slot.valid = false;
            return true;
        }

        // Consume the normalized, program-resolved recipe directly. It is
        // the source for both texture references and BDA material rows.
        const graph::mtl::MaterialRecipe &effective_recipe =
            row->cached_effective_recipe;
        const graph::mtl::MaterialRecipe &material_binding_recipe =
            row->cached_effective_recipe;
        if (row->cached_effective_recipe_hash == 0)
        {
            GLogWarning(
                "[RenderCollectSystem] Materialize failed: effective material recipe is not cached for %s",
                GetOwnerName(primitive_comp));
            return false;
        }

        if (!PrepareActivePlanResources(world, primitive_comp, material_program, effective_recipe))
            return false;

        if(!material_binding_recipe.material_ssbo_binding.IsValid())
        {
            if (ULRE_DEBUG_ON(Binding))
                GLogInfo("[ArenaTrace] materialize entry: material_binding_valid=false");
        }

        auto rdbs = world->GetSystem<RenderSceneUBOSystem>();
        if (!rdbs)
        {
            GLogWarning("[RenderCollectSystem] Materialize failed: RenderSceneUBOSystem missing for %s",
                        GetOwnerName(primitive_comp));
            return false;
        }

        // The binding recipe carries the primitive's active material row ID.
        const auto &asset_binding =
            material_binding_recipe.material_ssbo_binding;
        const uint32_t entity_data_index =
            asset_binding.IsValid() ? asset_binding.data_index : uint32_t(-1);

        // Fill the per-batch material address row for the shared material SSBO.
        // 每个材质 recipe 只声明一个共享材质数据 SSBO。
        if (asset_binding.IsValid())
        {
            // data_index is the active row ID in this type's shared material
            // buffer; translate it to the CPU/GPU address used by the current ABI.
            {
                row->material_row_gpu     = 0;

                auto *graphics_context = world->GetGraphicsContext();
                auto *material_domain = graphics_context
                    ? graphics_context->GetGlobalSSBOBufferRegistry()
                    : nullptr;
                graph::GlobalRowBufferInfo material_buffer{};
                if (!material_domain
                 || !material_domain->TryGetRowBuffer(
                        asset_binding.ssbo_id,
                        material_buffer))
                {
                    GLogError(
                        "[RenderCollectSystem] Materialize failed: material row buffer missing for %s type=%s ssbo_id=%u",
                        GetOwnerName(primitive_comp),
                        hgl::serialize::EnumNameOf(
                            asset_binding.ssbo_type),
                        asset_binding.ssbo_id);
                    return false;
                }
                if (!material_domain->IsActive(
                        asset_binding.ssbo_type,
                        asset_binding.data_index))
                {
                    GLogError(
                        "[RenderCollectSystem] Materialize failed: inactive material row ID for %s type=%s ssbo_id=%u data_index=%u",
                        GetOwnerName(primitive_comp),
                        hgl::serialize::EnumNameOf(
                            asset_binding.ssbo_type),
                        asset_binding.ssbo_id,
                        asset_binding.data_index);
                    return false;
                }
                if (material_buffer.global_ssbo_type
                        != asset_binding.ssbo_type
                 || material_buffer.gpu_base == 0
                 || material_buffer.row_bytes == 0
                 || material_buffer.row_capacity == 0
                 || asset_binding.data_index >= material_buffer.row_capacity)
                {
                    GLogError(
                        "[RenderCollectSystem] Materialize failed: material row buffer invalid for %s type=%s buffer_type=%s ssbo_id=%u data_index=%u capacity=%u row_bytes=%u",
                        GetOwnerName(primitive_comp),
                        hgl::serialize::EnumNameOf(
                            asset_binding.ssbo_type),
                        hgl::serialize::EnumNameOf(
                            material_buffer.global_ssbo_type),
                        asset_binding.ssbo_id,
                        asset_binding.data_index,
                        material_buffer.row_capacity,
                        material_buffer.row_bytes);
                    return false;
                }

                const uint64_t row_offset =
                    uint64_t(asset_binding.data_index) * material_buffer.row_bytes;
                row->material_row_gpu = material_buffer.gpu_base + row_offset;
            }
        }

        // data_index（行号）仍按 data_index VALUE 发布——行表/行尾镜像共用。
        row->data_index_row =
            entity_data_index != uint32_t(-1) ? entity_data_index : 0u;
        // 问题 8(b)：已物化的共享行，发布行号必须等于身份键的授权行号（未物化哨兵/自有行跳过）。
        MaterialRuntimeTable::VerifyWrittenDataIndexRow(*row, "RenderCollectSystem: 物化发布 data_index_row");

        auto *texture_graphics_context = world->GetGraphicsContext();
        auto *texture_registry = texture_graphics_context
            ? texture_graphics_context->GetSSBOBufferRegistry()
            : nullptr;

        graph::mtl::ResolvedMaterial texture_definition{};
        graph::mtl::MaterialTextureReferenceLayout texture_layout{};
        const bool has_texture_definition =
            ResolveResolvedMaterial(effective_recipe, texture_definition);

        if (!has_texture_definition
         || !graph::mtl::BuildMaterialTextureReferenceLayout(
                texture_definition,
                texture_layout))
        {
            GLogWarning(
                "[RenderCollectSystem] Materialize failed: texture definition lookup/layout failed for %s definition=%s",
                GetOwnerName(primitive_comp),
                effective_recipe.recipe_name.c_str());
            return false;
        }

        if (texture_registry)
            texture_registry->CollectRetiredMaterialTextureConfigurations(
                world->GetRenderSubmissionSerial());

        const uint64_t retire_epoch =
            static_cast<uint64_t>(world->GetRenderSubmissionSerial())
            + graph::MaterialTextureConfigurationRetireEpochDelay;

        if (texture_layout.HasReferences())
        {
            ValueArray<graph::mtl::MaterialTextureReference> references;
            references.Resize(static_cast<int>(texture_layout.reference_count));
            for (int i = 0; i < references.GetCount(); ++i)
                references[i] = {};

            for (size_t declaration_index = 0;
                 declaration_index < texture_definition.texture_declarations.size();
                 ++declaration_index)
            {
                const auto &declaration =
                    texture_definition.texture_declarations[declaration_index];
                const graph::mtl::RecipeTextureBinding *recipe_binding = nullptr;
                for (const auto &candidate : material_binding_recipe.textures)
                {
                    if (candidate.texture_name == declaration.name)
                    {
                        recipe_binding = &candidate;
                        break;
                    }
                }

                if (!recipe_binding)
                {
                    if (declaration.required)
                    {
                        GLogError(
                            "[RenderCollectSystem] Required texture binding missing: owner=%s texture=%s",
                            GetOwnerName(primitive_comp),
                            declaration.name.c_str());
                        return false;
                    }
                    continue;
                }

                if (recipe_binding->array_layer != 0
                 && !graph::mtl::IsMaterialTextureArraySampler(
                        declaration.sampler_type))
                {
                    GLogError(
                        "[RenderCollectSystem] Non-array texture received array layer: owner=%s texture=%s layer=%u",
                        GetOwnerName(primitive_comp),
                        declaration.name.c_str(),
                        recipe_binding->array_layer);
                    return false;
                }

                uint32_t handle = 0;
                if (!recipe_binding->resource_id.empty())
                {
                    handle = rdbs->GetBindlessHandle(
                        AnsiString(recipe_binding->resource_id.c_str()));

                    if (handle == 0)
                    {
                        // 兜底：配方里记录的身份还没登记 bindless（例如 RT 附件刚重建、
                        // 或资源由实体级覆盖提供）。实体级覆盖优先，其次配方级句柄。
                        const MaterialData *material_data =
                            FindMaterialDataOf(primitive_comp);
                        const auto *authoring = material_data
                            ? material_data->GetTextureResource(declaration.name)
                            : nullptr;
                        graph::TextureAsset *fallback_asset =
                            (authoring && authoring->IsReady())
                            ? authoring->asset
                            : recipe_binding->asset;
                        if (fallback_asset
                         && fallback_asset->IsReady())
                        {
                            const std::string fallback_id =
                                BuildTextureResourceId(fallback_asset);
                            handle = rdbs->GetBindlessHandle(
                                AnsiString(fallback_id.c_str()));
                        }
                    }
                }

                if (handle == 0 && declaration.required)
                {
                    // Step 5.2 占位保底：`LoadFailed` 是终态（`TextureStreamingSystem` 不自动重试），
                    // 若照样硬失败，一个坏文件就会让整个图元永远进不了收集（画面里静默少一块）。
                    // 这里改为留句柄 0 —— shader 侧 `SampleOptional()` / `if (handle == 0u)`
                    // 会取 fallback（白基色、未扰动法线），图元照常画出来。
                    graph::TextureAsset *slot_asset = recipe_binding->asset;
                    if (const MaterialData *slot_material =
                            FindMaterialDataOf(primitive_comp))
                    {
                        if (const auto *slot_authoring =
                                slot_material->GetTextureResource(declaration.name))
                            slot_asset = slot_authoring->asset;
                    }

                    if (!slot_asset
                     || slot_asset->GetPayloadState() != hgl::PayloadState::LoadFailed)
                    {
                        GLogError(
                            "[RenderCollectSystem] Required texture handle missing: owner=%s texture=%s resource=%s",
                            GetOwnerName(primitive_comp),
                            declaration.name.c_str(),
                            recipe_binding->resource_id.empty()
                                ? "<direct/empty>"
                                : recipe_binding->resource_id.c_str());
                        return false;
                    }

                    GLogWarning(
                        "[RenderCollectSystem] Required texture load failed, slot falls back to handle 0: owner=%s texture=%s",
                        GetOwnerName(primitive_comp),
                        declaration.name.c_str());
                }

                references[static_cast<int>(declaration_index)] = {
                    handle,
                    handle == 0 ? 0u : recipe_binding->array_layer};
            }

            hgl::hash::FNV1aHasher64 reference_hasher;
            reference_hasher << texture_layout.layout_hash
                             << texture_layout.reference_count;
            for (int i = 0; i < references.GetCount(); ++i)
            {
                reference_hasher << references[i].descriptor_index
                                 << references[i].array_layer;
            }
            const uint64_t reference_configuration_hash =
                reference_hasher;

            if (!texture_registry)
            {
                GLogError(
                    "[RenderCollectSystem] Materialize failed: SSBO registry missing for texture references owner=%s",
                    GetOwnerName(primitive_comp));
                return false;
            }

            const uint64_t pool_key =
                graph::MaterialTextureReferencePool::MakePoolKey(
                    texture_definition,
                    texture_layout);
            const auto old_allocation =
                row->material_texture_configuration;
            const bool old_allocation_live =
                old_allocation.IsValid()
             && texture_registry->IsMaterialTextureConfigurationValid(
                    old_allocation);
            const bool can_reuse =
                old_allocation_live
             && old_allocation.pool_key == pool_key
             && old_allocation.reference_count
                    == texture_layout.reference_count
             && old_allocation.row_stride == texture_layout.row_stride
             && row->material_texture_configuration_hash
                    == reference_configuration_hash;

            graph::MaterialTextureConfigurationAllocation new_allocation =
                old_allocation;
            if (!can_reuse)
            {
                if (!texture_registry->AcquireMaterialTextureConfiguration(
                        texture_definition,
                        texture_layout,
                        new_allocation))
                {
                    GLogError(
                        "[RenderCollectSystem] Materialize failed: texture configuration capacity exhausted owner=%s definition=%s",
                        GetOwnerName(primitive_comp),
                        effective_recipe.recipe_name.c_str());
                    return false;
                }
            }

            if (!can_reuse
             && !texture_registry->WriteMaterialTextureConfiguration(
                    new_allocation,
                    references.GetData(),
                    texture_layout.reference_count))
            {
                if (!can_reuse)
                    texture_registry->RetireMaterialTextureConfiguration(
                        new_allocation,
                        retire_epoch);
                GLogError(
                    "[RenderCollectSystem] Materialize failed: texture configuration write failed owner=%s",
                    GetOwnerName(primitive_comp));
                return false;
            }

            if (!can_reuse && old_allocation_live)
                texture_registry->RetireMaterialTextureConfiguration(
                    old_allocation,
                    retire_epoch);

            row->material_texture_configuration = new_allocation;
            row->material_texture_row_gpu =
                new_allocation.gpu_row;
            row->material_texture_zero_row_gpu =
                texture_registry->
                    GetMaterialTextureConfigurationZeroRowAddress(
                        texture_definition,
                        texture_layout);
            if (row->material_texture_zero_row_gpu == 0)
            {
                GLogError(
                    "[RenderCollectSystem] Materialize failed: texture configuration zero row unavailable owner=%s",
                    GetOwnerName(primitive_comp));
                return false;
            }
            row->material_texture_configuration_hash =
                reference_configuration_hash;

            // 纹理引用行落盘诊断（ULRE_DEBUG.toml 的 channels.binding = true）：逐语义打印**绑到行里的描述符索引**。
            // 「纹理不生效」的第一现场就是这里的 descriptor_index==0 ⇒ 着色器侧
            // SampleOptional 走 fallback（1.0）⇒ alpha 恒 1 ⇒ dither/blend 全看不出。
            if (ULRE_DEBUG_ON(Binding))
            {
                GLogInfo(
                    "[MaterialTextureReferences] owner=%s definition=%s row=%u references=%u gpu=0x%llx",
                    GetOwnerName(primitive_comp),
                    texture_definition.domain_name.c_str(),
                    new_allocation.row_index,
                    texture_layout.reference_count,
                    static_cast<unsigned long long>(
                        new_allocation.gpu_row));

                for (size_t i = 0;
                     i < texture_definition.texture_declarations.size();
                     ++i)
                {
                    const auto &declaration =
                        texture_definition.texture_declarations[i];
                    const auto &reference =
                        references[static_cast<int>(i)];

                    GLogInfo(
                        "[MaterialTextureReferences] texture=%s descriptor=%u layer=%u",
                        declaration.name.c_str(),
                        reference.descriptor_index,
                        reference.array_layer);
                }
            }
        }
        else
        {
            if (texture_registry
             && texture_registry->IsMaterialTextureConfigurationValid(
                    row->material_texture_configuration))
                texture_registry->RetireMaterialTextureConfiguration(
                    row->material_texture_configuration,
                    retire_epoch);
            row->material_texture_configuration = {};
            row->material_texture_row_gpu = 0;
            row->material_texture_zero_row_gpu = 0;
        }

        slot.runtime_dirty = false;
        slot.valid = false;
        slot.last_materialize_epoch = materialization.Epoch();
        return true;
    }


    void RenderCollectSystem::Update(float /*deltaTime*/)
    {
        if (!world)
            return;

        materialization.BeginFrame();
        if (auto *templates = world->GetModelTemplateMaterialTable())
            templates->CollectRetiredTemplates();

        // 本 pass 生效相机的 CameraInfo（离屏 pass = pass 相机，主帧 = 主相机）：
        // 每帧现取，**不缓存指针**——pass 之间相机会换（旧实现把指针缓存在安装期，pass 相机一上场就错）。
        const graph::CameraInfo *cameraInfo = world->GetActiveCameraInfo();

        if (!cameraInfo)
            return;

        auto& cache = world->GetRenderFrameCache();
        cache.cameraInfo = cameraInfo;
        hgl::graph::FrameDebugger::SetCameraSnapshot(cameraInfo);
        if(!world->IsCurrentPassShadow())
            if(auto *swapchain=dynamic_cast<hgl::graph::SwapchainRenderTarget *>(world->GetRenderTarget()))
                swapchain->RecordCaptureCamera(cameraInfo);

        // P1：每 pass 一次资格判定（只统计，不改变本帧任何行为）。
        ObserveCollectCacheEligibility();

        // A7b：本 pass 回退（错误）材质配方合成次数（诊断；默认 0 ⇒ 不打印任何东西，
        // 基线日志逐字不变）。
        g_fallback_recipe_builds = 0;

        const int active_mobility_filter = world ? world->GetActiveMobilityFilter() : -1;

        // T2-c + 阶段 1：图元枚举源 = **GeometryData 活跃集 ∪ 纯模板载体实体**。
        //
        // 纯模板载体 = 挂 `GPUModelRefComponent` 但**不挂** `GeometryData` 的实体
        //（整模型根实体：部件几何/材质都在模板池里，CPU 侧没有任何几何副本）。
        // 旧实现只扫 GeometryData 池 ⇒ 这类实体永不入批（模板展开也就永远不执行）。
        ValueArray<Entity *> primitives;
        {
            ValueArray<GeometryData *> geometry_rows;
            world->GetComponents<GeometryData>(geometry_rows);

            for (GeometryData *geometry_comp : geometry_rows)
                if (geometry_comp && geometry_comp->GetOwner())
                    primitives.Add(geometry_comp->GetOwner());

            // 先收集 ID 再取可变句柄（`ForEachAliveEntity` 只给 const 视图）。
            ValueArray<EntityID> carrier_ids;
            world->ForEachAliveEntity([&carrier_ids](const Entity *alive)
            {
                if (!alive || alive->GetComponent<GeometryData>() || !alive->GetComponent<GPUModelRefComponent>())
                    return;

                carrier_ids.Add(alive->GetEntityID());
            });

            for (EntityID carrier_id : carrier_ids)
                if (Entity *carrier = world->GetEntity(carrier_id))
                    primitives.Add(carrier);
        }

        // A4：材质运行期表（世界私有）——共享行 = 可共享的材质绑定；slot = 每实例状态。
        MaterialRuntimeTable *runtime_table = world->GetMaterialRuntimeTable();

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
        if (!world->IsCurrentPassShadow())
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
        bool any_material_work = false;
        bool any_possible_runtime_rows_visible = false;
        bool any_pending_resources = false;

        FrameProfiler::Count(FrameProfileCount::CollectPrimitives, primitives.GetCount());
        FrameProfiler::Count(FrameProfileCount::CollectCalls);

        cache.scene_primitives = static_cast<uint32_t>(primitives.GetCount());

        // P3：主帧收集快路径。命中即本 pass 的收集工作**全部完成** ⇒ 直接返回。
        // 未命中（任一前提不成立，或中途发现不一致而主动回退）⇒ 照原样跑完整收集，
        // 回退路径不留下任何半成品（见 `TryCachedMainFrameCollect` 的契约）。
        if (TryCachedMainFrameCollect(ambient_provider_changed))
            return;

        {
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
                    any_pending_resources = true;
                    any_material_work     = true;
                    ++cache.pending_geometry_resources;
                    if(trace) trace->pending_geometry=true;
                }
                if (HasPendingMaterialResources(owner))
                {
                    any_pending_resources = true;
                    any_material_work     = true;
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
                       world->IsEntityVisible(entity->GetEntityID())
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
                TransformAccessor transform = world->GetTransformByEntity(entity->GetEntityID());
                if (!transform.IsValid() || static_cast<int>(transform.GetMobility()) != active_mobility_filter)
                    continue;
            }

            // 阶段 1：在进入收集主循环之前，确保所有有效渲染几何实体已绑定单图元模型模板
            if (ambient_provider_changed)
            {
                const auto *ref = entity->GetComponent<GPUModelRefComponent>();
                if (!ref || ref->automatically_normalized)
                    world->GetOrCreateMaterialRuntimeSlot(entity).program_dirty = true;
            }
            if (!EnsurePrimitiveModelTemplate(entity))
            {
                any_material_work = true;
                any_pending_resources = true;
            }

            // 阶段 1/2：模板载体实体（挂有效 GPUModelRefComponent）
            // 若在 ModelTemplateMaterialTable 中已注册绑定（自包含 Shading Groups），或无 GeometryData，
            // 则其部件几何与材质常驻 GPU SSBO，无需参与单图元 slot 预扫描。
            const GPUModelRefComponent *scan_model_ref = entity->GetComponent<GPUModelRefComponent>();
            if (scan_model_ref && scan_model_ref->template_id != graph::ssbo::kInvalidModelTemplateID)
            {
                const ModelTemplateMaterialTable *template_mat_table = world->GetModelTemplateMaterialTable();
                const bool has_template_binding = template_mat_table && template_mat_table->FindBinding(scan_model_ref->template_id) != nullptr;
                if (has_template_binding || !entity->GetComponent<GeometryData>())
                    continue;
            }

            // 阴影 pass 的 caster 能力（CanCastShadow）与距离裁剪（in_shadow_range）已由表的
            // CollectForCurrentPass 条件要求表达 ⇒ 这里不再重复判据。

            // A4：材质运行期状态 = **每实例 slot**（世界表里按实体稀疏存放）+
            // **共享行**（绑定）。这里取/建 slot，再经行取 program 与缓存哈希。
            MaterialRuntimeSlot &material_slot =
                world->GetOrCreateMaterialRuntimeSlot(entity);

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

            any_material_work |= needs_work;
            any_possible_runtime_rows_visible |= possible_runtime_rows;
        }

        }// CollectPreScan 子槽

        if (any_material_work && any_possible_runtime_rows_visible)
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
                       world->IsEntityVisible(entity->GetEntityID())
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

            TransformAccessor transform = world->GetTransformByEntity(entity->GetEntityID());

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

            // 阴影 pass 的 caster 能力与距离裁剪已由表的 CollectForCurrentPass 条件要求表达
            // ⇒ 这里不再重复判据（原两条 continue 随之删除）。

            // 阶段 1/2：模板载体实体（挂有效 GPUModelRefComponent）快线分流
            // 按 ModelTemplateMaterialTable 中已注册的各 Shading Group 独立投递载体项（完全绕过传统单图元材质物化）
            const GPUModelRefComponent *pure_model_ref = entity->GetComponent<GPUModelRefComponent>();
            const ModelTemplateMaterialTable *template_mat_table = world->GetModelTemplateMaterialTable();
            const TemplateRenderBinding *binding = (pure_model_ref && template_mat_table)
                ? template_mat_table->FindBinding(pure_model_ref->template_id) : nullptr;

            const bool is_model_template = pure_model_ref
                && pure_model_ref->template_id != graph::ssbo::kInvalidModelTemplateID;

            if (is_model_template)
            {
                if (!binding || binding->groups.IsEmpty())
                {
                    any_material_work = true;
                    cache.RecordDrop(entity_name, entity_id.index, "RenderCollect", "NoTemplateBinding", "Model template has no render binding");
                    continue;
                }

                const bool shadow_pass = world->IsCurrentPassShadow();
                auto *render_target = world->GetRenderTarget();
                auto *render_pass = render_target ? render_target->GetRenderPass() : nullptr;
                MaterialRuntimeTable *current_runtime_table = GetRuntimeTable(world);
                MaterialVariantTable *variant_table = GetVariantTable(world);

                const uint32_t transform_id = transform.GetID();
                const uint32_t pass_index = shadow_pass ? 1u : 0u;

                // 提取材质 4-ID 事实（支持覆盖优先，默认回退模板 group 0）
                uint32_t fact_material_id = 0u;
                uint32_t fact_texture_id = 0u;
                MaterialRuntimeRowID active_row_id = pure_model_ref->GetOverrideMaterialRow();
                if (active_row_id == INVALID_MATERIAL_RUNTIME_ROW_ID && !binding->groups.IsEmpty())
                {
                    active_row_id = binding->groups[0].runtime_row;
                }
                if (active_row_id != INVALID_MATERIAL_RUNTIME_ROW_ID)
                {
                    if (const MaterialRuntimeRow *mat_row = current_runtime_table ? current_runtime_table->Get(active_row_id) : nullptr)
                    {
                        if (mat_row->data_index_row != uint32_t(-1))
                            fact_material_id = mat_row->data_index_row;
                        if (mat_row->material_texture_configuration.IsValid())
                            fact_texture_id = mat_row->material_texture_configuration.row_index;
                    }
                }

                // 发布模板实体的权威事实（template_id, part_visibility_mask, etc.）
                PublishPrimitiveFacts(entity_id, transform_id, 0u, fact_material_id, fact_texture_id, pass_index, nullptr);

                uint32_t collected_groups = 0;
                bool group_failed = false;
                auto drop_group = [&](uint32_t group, const char *reason, const char *message)
                {
                    group_failed = true;
                    any_material_work = true;
                    ++cache.dropped_other;
                    cache.RecordDrop(entity_name, entity_id.index, "RenderCollect", reason, message);
                    LogWarning(u8"Model template group skipped: owner=%s template=%u group=%u reason=%s",
                               entity_name, pure_model_ref->template_id, group, reason);
                    if (trace)
                        ::strncpy(trace->collect_reason, reason, sizeof(trace->collect_reason) - 1);
                };

                for (uint32_t g = 0; g < static_cast<uint32_t>(binding->groups.GetCount()); ++g)
                {
                    const TemplateShadingGroupDesc &grp_desc = binding->groups[g];

                    if (shadow_pass && !grp_desc.casts_shadow)
                        continue;

                    MaterialRuntimeRowID row_id = grp_desc.runtime_row;
                    if (pure_model_ref->HasMaterialOverride())
                    {
                        row_id = pure_model_ref->GetOverrideMaterialRow();
                    }

                    const MaterialRuntimeRow *row = current_runtime_table ? current_runtime_table->Get(row_id) : nullptr;
                    if (!row)
                    {
                        drop_group(g, "MissingMaterialRuntimeRow", "Template group material runtime row is unavailable");
                        continue;
                    }

                    graph::PrimitiveType primitive_type;
                    const graph::GeometryVertexFormat *vertex_format = grp_desc.vertex_format;
                    if (!vertex_format)
                        GetPrimitiveProgramBuildInputs(entity, primitive_type, vertex_format);
                    graph::ShaderProgram *program = EnsureTemplateGroupProgram(
                        world, variant_table, current_runtime_table, row_id, shadow_pass,
                        grp_desc.primitive_type, vertex_format);
                    if (!program || !render_pass)
                    {
                        drop_group(g, "TemplateProgramUnavailable", "Template group program or render pass is unavailable");
                        continue;
                    }

                    graph::mtl::MaterialRecipe effective_recipe = shadow_pass
                        ? row->shadow_cached_normalized_recipe
                        : row->cached_effective_recipe;

                    if (!shadow_pass && row->recipe_hash == row->cached_effective_recipe_hash)
                        effective_recipe = row->cached_normalized_recipe;

                    graph::mtl::MaterialPipelineConfig resolved_pipeline_config{};
                    graph::Pipeline *resolved_pipeline = render_pass->CreatePipeline(
                        program, effective_recipe, &resolved_pipeline_config);

                    if (!resolved_pipeline)
                    {
                        drop_group(g, "TemplatePipelineUnavailable", "Template group pipeline could not be resolved");
                        continue;
                    }

                    Entity *primitive_comp = entity;
                    world->GetMaterialRuntimePipelineTable().Set(primitive_comp->GetEntityID(),
                                                                 render_pass,
                                                                 resolved_pipeline,
                                                                 resolved_pipeline_config,
                                                                 g, program);

                    auto item = std::make_unique<ModelTemplateRenderItem>(
                        entity_id,
                        transform,
                        row_id,
                        g,
                        pure_model_ref->template_id,
                        render_pass,
                        resolved_pipeline,
                        resolved_pipeline_config,
                        world);

                    item->UpdateWorldMatrix();

                    cache.renderItems.push_back(std::move(item));
                    cache.renderableCount++;
                    ++added;
                    ++cache.collected_items;
                    ++collected_groups;
                }

                if (trace)
                    ::strncpy(trace->collect_status,
                              group_failed ? (collected_groups ? "partial" : "dropped")
                                           : (collected_groups ? "collected" : "filtered"),
                              sizeof(trace->collect_status) - 1);
                shadow_pass_idsum += (static_cast<uint64_t>(entity_id.index) << 16) | entity_id.generation;

                continue;
            }

            // 未就绪实体等待后续帧重新归一化，不回退旧单图元收集路线。
            // 经过预扫描 EnsurePrimitiveModelTemplate，所有合法图元实体均已完成单图元模型模板晋升。
            // 若仍有非模板实体进入此处，记录 Drop 并跳过，不再保留旧单图元物化与收集分支。
            if (trace)
            {
                ::strncpy(trace->collect_status, "dropped", sizeof(trace->collect_status) - 1);
                ::strncpy(trace->collect_reason, "no_valid_model_template", sizeof(trace->collect_reason) - 1);
            }
            cache.RecordDrop(entity_name, entity_id.index, "RenderCollect", "NoModelTemplate", "Entity has no valid model template or binding");
            any_material_work = true;
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
                        world->IsCurrentPassShadow() ? 1 : 0, g_fallback_recipe_builds);
        }

        const bool s6_collect_log = ULRE_DEBUG_ON(Shadow);
        if (s6_collect_log && world && world->IsCurrentPassShadow())
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
        if (world->IsMainFrameTopologyPass())
        {
            last_main_collect_steady    = !any_material_work && !any_pending_resources;
            cache.is_collect_steady     = last_main_collect_steady;
            cache.has_pending_resources = any_pending_resources;
        }
    }
}//namespace hgl::ecs
