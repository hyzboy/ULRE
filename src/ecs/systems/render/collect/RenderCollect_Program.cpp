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

    namespace detail::render_collect
    {
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
            return world ? world->RenderData().Variants() : nullptr;
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

            ECSContext *context = primitive_comp ? primitive_comp->GetContextAs<hgl::ecs::ECSContext>() : nullptr;
            graph::GraphicsContext *graphics = context ? context->Environment().GetGraphicsContext() : nullptr;

            if (!graphics)
            {
                auto *render_context = context ? context->Environment().GetRenderContext() : nullptr;
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

    }

    /// 取配方对应的材质定义：由领域契约 + 配方合成。失败（配方无有效领域）⇒
    /// 用回退领域合成，绝不剔除图元。
    bool detail::render_collect::ResolveResolvedMaterial(
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

    namespace detail::render_collect
    {
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
            out_graphics = world->Environment().GetGraphicsContext();
            if (!out_graphics)
            {
                auto *render_context = world->Environment().GetRenderContext();
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
        // → `ECSContext::Environment().GetAmbientProviderName()`。材质不再声明它 —— 环境光的算法与
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

            const AnsiString &name = world->Environment().GetAmbientProviderName();
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

    }

    graph::ShaderProgram *detail::render_collect::EnsureTemplateGroupProgram(
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
        else
        {
            row->forward_variant = variant_id;
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
        if (world->Runner().PassScope().IsShadowPass())
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
            world->RenderData().InvalidateEntityRuntimePipeline(primitive_comp->GetEntityID());
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
        const bool shadow_pass = world->Runner().PassScope().IsShadowPass();
        graph::ShaderProgram *program = GetCurrentPassProgram(
            GetVariantTable(world), row, shadow_pass);
        if (!program)
            return false;

        // 当前渲染目标唯一权威：world->Environment().GetRenderTarget()（RenderContext 副本已删除；
        // RenderTo 切 RT 时会同步本世界的 render_target 指针）
        auto *render_target = world->Environment().GetRenderTarget();
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
        world->RenderData().GetMaterialRuntimePipelineTable().Set(primitive_comp->GetEntityID(),
                                                     render_pass,
                                                     resolved_pipeline,
                                                     resolved_pipeline_config);
        return true;
    }

}
