#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/support/RenderStrategyParity.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/RenderResource.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/ShadowProxy.h>
#include<hgl/ecs/support/PrimitiveState.h>
#include<hgl/ecs/support/MaterialRuntimeTable.h>
#include<hgl/ecs/support/MaterialVariantTable.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/core/InstancedPrimitiveRenderItem.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include <cstdlib>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include<hgl/ecs/support/VisibilityDataStorage.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/SSBOBufferRegistry.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>

#include<hgl/graph/ssbo/MaterialSSBOLayout.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/util/hash/FNV1a.h>
#include<hgl/log/Log.h>
#include<hgl/vk/VKRenderPass.h>
#include<glm/glm.hpp>
#include<cstring>

namespace hgl::ecs
{
    namespace
    {
        const char *GetOwnerName(const Entity *entity)
        {
            if (!entity)
                return "<null-entity>";

            return entity->GetName().c_str();
        }

        /// A2/A5b 查询：实体的**材质数据层**组件（无实体/无世界时 nullptr）。
        MaterialData *FindMaterialDataOf(Entity *entity)
        {
            ECSContext *context = entity ? entity->GetContext() : nullptr;

            return context ? context->GetMaterialData(entity) : nullptr;
        }

        /// 材质数据层组件（无则创建）。渲染侧需要读出授权状态时，数据层必须存在。
        MaterialData *EnsureMaterialDataOf(Entity *entity)
        {
            ECSContext *context = entity ? entity->GetContext() : nullptr;

            return context ? context->GetOrCreateMaterialData(entity) : nullptr;
        }

        /// A5a 查询：实体的**几何资产组件**（无实体/无世界/未挂载时 nullptr）。
        GeometryData *FindGeometryOf(Entity *entity)
        {
            ECSContext *context = entity ? entity->GetContext() : nullptr;

            return context ? context->GetGeometryData(entity) : nullptr;
        }

        /// 材质授权代数（A2 之前挂在图元组件上，随授权状态一并迁入数据层）。
        uint32_t MaterialAuthoredGenerationOf(Entity *entity)
        {
            MaterialData *material_data = EnsureMaterialDataOf(entity);

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

        MaterialRuntimeTable *GetRuntimeTable(ECSContext *world)
        {
            return world ? world->GetMaterialRuntimeTable() : nullptr;
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

        // D9：阴影 pass 跳过路径的收敛参数。
        // 持续跳过（行未就绪/解析失败等）时原本每帧 bump 静态级联 revision →
        // 每帧全量重画且无任何日志。前 kShadowRetryFullBumpFrames 帧保持每帧 bump
        // （正常情况下一两帧内就收敛），超过后降频到每 kShadowRetryBumpPeriod 帧一次，
        // 并在跨越阈值时报一次错——既不刷屏也不放弃自愈。
        constexpr uint32_t kShadowRetryFullBumpFrames = 120;
        constexpr uint32_t kShadowRetryBumpPeriod     = 60;


        bool EnsureRuntimeGeometryFromAsset(ECSContext *world,
                                            Entity *primitive_comp,
                                            const MaterialRuntimeSlot &slot,
                                            const MaterialRuntimeRow *row)
        {
            if (!world || !primitive_comp || !row)
            {
                GLogError("[RenderPrimitiveCollectSystem] EnsureRuntimeGeometryFromAsset precondition failed world=%p primitive=%p row=%p",
                          world,
                          primitive_comp,
                          static_cast<const void *>(row));
                return false;
            }

            GeometryData *geometry = FindGeometryOf(primitive_comp);
            const auto *asset = geometry ? geometry->GetPrimitiveAsset() : nullptr;
            if (!asset)
                return true;

            // SSBO 顶点方案无 VIL，GeometryDataBuffer 的内容只由 geometry 决定，
            // program 参数仅作绑定哨兵。阴影 pass 先于主帧首解析时 forward
            // program 尚为空，退回 shadow 槽保证绑定可建；哨兵始终以 forward
            // program 为准，避免两个槽的程序指针交替触发几何缓冲销毁重建。
            MaterialVariantTable *variant_table = GetVariantTable(world);
            const MaterialRuntimeRow *forward_row = row;
            auto *material = GetForwardProgram(variant_table, forward_row);
            if (!material)
                material = GetShadowProgram(variant_table, forward_row);
            if (!material)
            {
                GLogError("[RenderPrimitiveCollectSystem] EnsureRuntimeGeometryFromAsset failed: material program null owner=%s valid=%d program_dirty=%d runtime_dirty=%d",
                          GetOwnerName(primitive_comp),
                          slot.valid ? 1 : 0,
                          slot.program_dirty ? 1 : 0,
                          slot.runtime_dirty ? 1 : 0);
                return false;
            }
            return geometry->EnsureRuntimeGeometryBinding(material);
        }

        inline graph::GlobalSSBOType ResolveMaterialSSBORequirementType(
            const graph::mtl::ShaderResourceSlot &req) noexcept
        {
            return req.global_ssbo_type;
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
            const uint32_t needs = EvaluateRenderNeed(
                owner ? owner->GetComponentMask() : 0u, facts.ToMask());

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
                GLogWarning(u8"[MaterialFallback] marker row unavailable (no graphics device or pool not ready); fallback material %s keeps rendering without marker color: %s",
                            out_recipe.mtl_def_id.c_str(), GetOwnerName(primitive_comp));
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

        bool BuildResolvedRecipe(Entity *primitive_comp,
                                 const graph::ShaderProgram *material_program,
                                 graph::mtl::MaterialRecipe &out_recipe)
        {
            if (!primitive_comp)
                return false;

            MaterialData *material_data = EnsureMaterialDataOf(primitive_comp);

            if (!material_data)
                return false;

            // asset 里的默认配方是基底，数据层里的配方覆盖是覆盖源（配方来源 = 实体的
            // GeometryData 组件；A5a 起由几何组件持有 asset）。
            const GeometryData *geometry = FindGeometryOf(primitive_comp);

            if (material_data->BuildResolvedRecipe(out_recipe,
                                                   material_program,
                                                   geometry ? geometry->GetAssetMaterialRecipe() : nullptr))
                return AttachFallbackMarkerColor(primitive_comp, out_recipe);

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
                        out_recipe.mtl_def_id.c_str(),
                        rule.marker_color[0], rule.marker_color[1],
                        rule.marker_color[2], rule.marker_color[3],
                        GetOwnerName(primitive_comp));

            return AttachFallbackMarkerColor(primitive_comp, out_recipe);
        }

        // program 解析（forward 与 ShadowCaster 双槽）共用的构建上下文输入：
        // primitive 类型与顶点格式来自 asset（无 asset 时 Triangles + 空格式）。
        void GetPrimitiveProgramBuildInputs(
            Entity *primitive_comp,
            graph::PrimitiveType &out_primitive_type,
            const graph::GeometryVertexFormat *&out_geometry_vertex_format)
        {
            out_primitive_type = graph::PrimitiveType::Triangles;
            out_geometry_vertex_format = nullptr;

            const GeometryData *geometry = FindGeometryOf(primitive_comp);
            if (const auto *asset = geometry ? geometry->GetPrimitiveAsset() : nullptr)
            {
                if (auto *asset_geometry = asset->GetGeometry())
                    out_geometry_vertex_format =
                        &asset_geometry->GetGeometryVertexFormat();
                out_primitive_type = asset->GetPrimitiveType();
            }
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

        bool ResolveMaterialDefinition(
            const graph::mtl::MaterialRecipe &recipe,
            graph::mtl::MaterialDefinition &out_definition)
        {
            if (!recipe.mtl_def_id.empty()
             && graph::mtl::TryGetMaterialDefinitionByID(
                    recipe.mtl_def_id,
                    out_definition))
                return true;

            return graph::mtl::TryGetMaterialDefinitionByID(
                graph::mtl::GetFallbackMaterialDefinitionID(),
                out_definition);
        }

        bool ValidateMaterialRecipeForRuntime(
            const graph::mtl::MaterialRecipe &recipe,
            const graph::ShaderProgram *material_program,
            const graph::mtl::MaterialDefinition &definition,
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
                        definition.definition_id.c_str());
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

            for (const auto &req :
                 material_program->GetShaderResourceSchema().resources)
            {
                if (req.semantic
                        != graph::mtl::DescriptorSemantic::MaterialPrivateData)
                    continue;

                const auto &binding = recipe.material_ssbo_binding;
                if (!binding.IsValid()
                 || binding.ssbo_type
                        != ResolveMaterialSSBORequirementType(req))
                {
                    GLogError(
                        "[MaterialBinding] Material data binding missing or invalid owner=%s descriptor=%s type=%s",
                        owner_name ? owner_name : "<null>",
                        req.name.empty() ? "<unnamed>" : req.name.c_str(),
                        graph::GetGlobalSSBOTypeName(
                            ResolveMaterialSSBORequirementType(req)));
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

            graph::mtl::MaterialDefinition definition{};
            if (!ResolveMaterialDefinition(active_recipe, definition)
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
                EnsureMaterialDataOf(primitive_comp);

            for (const auto &binding : active_recipe.textures)
            {
                if (binding.resource_id.empty())
                    continue;

                const auto *resource = material_data
                    ? material_data->GetTextureResource(binding.texture_name)
                    : nullptr;
                if (!resource
                 || !resource->texture
                 || !resource->sampler
                 || !bindless_mgr)
                {
                    GLogError(
                        "[DeferredResource] Texture acquisition failed: owner=%s texture=%s binding=%d resource=%d bindless=%d",
                        owner_name,
                        binding.texture_name.c_str(),
                        1,
                        resource ? 1 : 0,
                        bindless_mgr ? 1 : 0);
                    return false;
                }

                const std::string resource_id =
                    resource->resource_id.empty()
                        ? BuildTextureResourceId(resource->texture)
                        : resource->resource_id;
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
                    resource->texture,
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
                row->material_row_cpu = nullptr;
                row->material_row_gpu = 0;
                row->material_texture_row_cpu = nullptr;
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

        /// 实体 + 世界 ⇒ 收集策略（表是唯一判据）
        CollectStrategy EvaluateCollectStrategy(ECSContext *world,const GeometryData *geometry_comp)
        {
            CollectStrategy out;

            const Entity *owner = geometry_comp->GetOwner();

            out.facts.entity_visible      = world->IsEntityVisible(geometry_comp->GetOwnerID());
            out.facts.has_owner           = (owner != nullptr);
            out.facts.renderable          = CanRender(owner);
            out.facts.has_material_source = HasAnyMaterialSource(owner);
            out.facts.cast_shadow         = CanCastShadow(owner);
            out.facts.receive_shadow      = CanReceiveShadow(owner);
            out.facts.shadow_pass         = world->IsCurrentPassShadow();

            if (out.facts.shadow_pass)
            {
                const float max_dist = GetShadowMaxDistance(owner);

                if (max_dist > 0.0f && world->HasShadowOrigin())
                {
                    const TransformAccessor transform =
                        world->GetTransformByEntity(geometry_comp->GetOwnerID());

                    if (transform.IsValid())
                    {
                        const glm::vec3 diff =
                            transform.GetWorldPosition() - world->GetShadowOrigin();

                        out.in_shadow_range = glm::dot(diff,diff) <= max_dist * max_dist;
                    }
                }
            }

            out.facts.in_shadow_range = out.in_shadow_range;
            out.needs = EvaluateRenderNeed(owner ? owner->GetComponentMask() : 0u,
                                           out.facts.ToMask());

            return out;
        }
    }

    RenderPrimitiveCollectSystem::RenderPrimitiveCollectSystem(const std::string& name)
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
    bool RenderPrimitiveCollectSystem::ResolveShadowCasterProgram(Entity *primitive_comp,
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
            GLogWarning("[RenderPrimitiveCollectSystem] ShadowCaster resolve failed: graphics/material manager null for %s",
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
            GLogWarning("[RenderPrimitiveCollectSystem] ShadowCaster BuildResolvedRecipe failed for %s",
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
            const uint64_t forward_build_context =
                graph::mtl::HashMaterialProgramBuildContext(
                    primitive_type,
                    geometry_vertex_format,
                    graphics->GetPhysicalDeviceProfile(),
                    GetEffectiveForwardPurpose(primitive_comp));

            AssignSlotRow(runtime_table, slot,
                          MaterialRuntimeKey{forward_build_context, recipe_hash,
                                             ResolveRowDataIndex(effective_recipe)});
        }

        const MaterialVariantID shadow_variant = variant_table
            ? variant_table->Intern(MaterialVariantKey{build_context_hash, recipe_hash})
            : INVALID_MATERIAL_VARIANT_ID;

        if (shadow_variant == INVALID_MATERIAL_VARIANT_ID)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] ShadowCaster variant unavailable/full for %s build_context=%llu recipe=%llu",
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

        // purpose 固定为 ShadowDepth：SelectCurrentSceneRenderTemplateRequest
        // 依此分派 ShadowCasterOpaque/Masked（按 recipe 的 alpha_test）。
        graph::mtl::MaterialDefinitionBuildRequest mtl_request{};
        mtl_request.recipe = effective_recipe;
        mtl_request.primitive_type = primitive_type;
        mtl_request.geometry_vertex_format = geometry_vertex_format;
        mtl_request.shader_program_purpose =
            graph::mtl::ShaderProgramPurpose::ShadowDepth;
        graph::mtl::MaterialDefinition template_definition{};
        if (!graph::mtl::TryGetMaterialDefinitionByID(
                effective_recipe.mtl_def_id, template_definition)
         && !graph::mtl::TryGetMaterialDefinitionByID(
                graph::mtl::GetFallbackMaterialDefinitionID(),
                template_definition))
        {
            GLogWarning(
                "[RenderPrimitiveCollectSystem] ShadowCaster cannot select template for material=%s",
                effective_recipe.mtl_def_id.c_str());
            return false;
        }
        if (!graph::SelectCurrentSceneRenderTemplateRequest(
                template_definition, mtl_request,
                mtl_request.render_template_request))
        {
            GLogWarning(
                "[RenderPrimitiveCollectSystem] ShadowCaster template selection failed for material=%s",
                effective_recipe.mtl_def_id.c_str());
            return false;
        }

        graph::ShaderProgram *resolved_program =
            material_manager->AcquireShaderProgram(mtl_request);
        if (!resolved_program)
        {
            GLogWarning(
                "[RenderPrimitiveCollectSystem] ShadowCaster AcquireShaderProgram failed for %s recipe=%s mtl_def_id=%s",
                GetOwnerName(primitive_comp),
                effective_recipe.recipe_name.c_str(),
                effective_recipe.mtl_def_id.c_str());
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

    bool RenderPrimitiveCollectSystem::ResolveMaterialProgramForPrimitive(Entity *primitive_comp,
                                                                          MaterialRuntimeSlot &slot)
    {
        if (!world || !primitive_comp)
            return false;

        // A1：阴影 pass 一律走专用 ShadowCaster 程序槽；下方解析链只服务
        // forward pass，不感知当前 pass。
        if (world->IsCurrentPassShadow())
            return ResolveShadowCasterProgram(primitive_comp, slot);

        return ResolveForwardProgram(primitive_comp, slot);
    }

    // Forward 槽解析（主帧着色程序）。阴影 pass 中 masked caster 的行物化
    // 也会借道此处（见主循环 A1-4 分支）——纹理行是 per-primitive 共享
    // 状态，与 program 无关。
    bool RenderPrimitiveCollectSystem::ResolveForwardProgram(Entity *primitive_comp,
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
            GLogWarning("[RenderPrimitiveCollectSystem] BuildResolvedRecipe failed for %s",
                        GetOwnerName(primitive_comp));
            return false;
        }

        const uint64_t recipe_hash = graph::mtl::HashMaterialRecipe(effective_recipe);
        graph::GraphicsContext *graphics = nullptr;
        graph::ShaderProgramManager *material_manager = nullptr;
        if (!ResolveGraphicsAndMaterialManager(world, graphics, material_manager))
        {
            GLogWarning("[RenderPrimitiveCollectSystem] ResolveMaterialProgram failed: graphics/material manager null for %s",
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

        const uint64_t build_context_hash =
            graph::mtl::HashMaterialProgramBuildContext(
                primitive_type,
                geometry_vertex_format,
                graphics->GetPhysicalDeviceProfile(),
                effective_purpose);

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
            // 即便 program 身份不变（例如仅 double_sided/cull 变化）也必须重建
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
        graph::mtl::MaterialDefinitionBuildRequest mtl_request{};
        mtl_request.recipe = effective_recipe;
        mtl_request.primitive_type = primitive_type;
        mtl_request.geometry_vertex_format = geometry_vertex_format;
        mtl_request.shader_program_purpose = effective_purpose;
        graph::mtl::MaterialDefinition template_definition{};
        if (!graph::mtl::TryGetMaterialDefinitionByID(
                effective_recipe.mtl_def_id, template_definition)
         && !graph::mtl::TryGetMaterialDefinitionByID(
                graph::mtl::GetFallbackMaterialDefinitionID(),
                template_definition))
        {
            GLogWarning(
                "[RenderPrimitiveCollectSystem] Cannot select template for material=%s",
                effective_recipe.mtl_def_id.c_str());
            return false;
        }
        if (!graph::SelectCurrentSceneRenderTemplateRequest(
                template_definition, mtl_request,
                mtl_request.render_template_request))
        {
            GLogWarning(
                "[RenderPrimitiveCollectSystem] Template selection failed for material=%s",
                effective_recipe.mtl_def_id.c_str());
            return false;
        }
        graph::ShaderProgram *resolved_program =
            material_manager->AcquireShaderProgram(mtl_request);

        if (!resolved_program)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] AcquireShaderProgram failed for %s recipe=%s mtl_def_id=%s",
                        GetOwnerName(primitive_comp),
                        effective_recipe.recipe_name.c_str(),
                        effective_recipe.mtl_def_id.c_str());
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
                "[RenderPrimitiveCollectSystem] Direct material recipe validation failed for %s",
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

        if (auto rdbs = world->GetSystem<RenderSceneUBOSystem>())
        {
            for (const auto &req : resolved_program->GetShaderResourceSchema().resources)
            {
                if (req.semantic != graph::mtl::DescriptorSemantic::MaterialPrivateData)
                    continue;

                const graph::GlobalSSBOType global_ssbo_type =
                    ResolveMaterialSSBORequirementType(req);
                const uint32_t stride = graph::GetGlobalSSBOTypeStructStride(global_ssbo_type);
                if (stride == 0)
                    continue;

                rdbs->RegisterMaterialStructLayout(global_ssbo_type, req.ssbo_id, stride);
            }
        }

        // A3：登记/复用该键的变体记录并落 program（记录 program 只在解析路径写）。
        // 变体键 = (build context, **recipe 身份**)：同 build context 下**不同材质必须
        // 落不同记录**，否则后解析者的 program 会覆盖先解析者（键少一维的 bug）。
        // recipe_hash 取自本次解析所用的 effective recipe（见上方
        // HashMaterialRecipe(effective_recipe) 那处比对）。
        const MaterialVariantID forward_variant =
            variant_table->Intern(MaterialVariantKey{build_context_hash, recipe_hash});
        if (forward_variant == INVALID_MATERIAL_VARIANT_ID)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] forward variant unavailable/full for %s build_context=%llu recipe=%llu",
                        GetOwnerName(primitive_comp),
                        (unsigned long long)build_context_hash,
                        (unsigned long long)recipe_hash);
            return false;
        }

        // A4：登记/复用**运行期共享行**（intern + refcount；同键多实体共用一行）。
        // 键 = (program 身份, 配方身份, 自有材质数据行)；`AssignSlotRow` 负责换行时的
        // 旧行释放（归零回收 + 退休 GPU 绑定）与"同键复用抵消这次引用"。
        // 注意：Intern 可能让行/记录两个容器扩容搬家 ⇒ 之后一律重新取指针。
        const MaterialRuntimeKey runtime_key{
            build_context_hash,
            recipe_hash,
            ResolveRowDataIndex(material_binding_recipe)};

        const MaterialRuntimeRowID row_id = AssignSlotRow(runtime_table, slot, runtime_key);
        if (row_id == INVALID_MATERIAL_RUNTIME_ROW_ID)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] runtime row unavailable/full for %s build_context=%llu recipe=%llu data_index=%u",
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

    bool RenderPrimitiveCollectSystem::ResolveRuntimePipelineForPrimitive(Entity *primitive_comp,
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

        graph::Pipeline *resolved_pipeline = render_pass->CreatePipeline(program,
                                                                         effective_recipe);
        if (!resolved_pipeline)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] ResolveRuntimePipeline failed: CreatePipeline failed for %s material=%s",
                        GetOwnerName(primitive_comp),
                        program->GetName().c_str());
            return false;
        }

        slot.runtime_pipeline_pass = render_pass;
        slot.runtime_pipeline      = resolved_pipeline;
        return true;
    }

    bool RenderPrimitiveCollectSystem::MaterializeRecipeRowsForPrimitive(Entity *primitive_comp,
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
         || !graph::mtl::MaterialRequiresRecipeRuntimeRows(
                material_program->GetShaderResourceSchema()))
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
                "[RenderPrimitiveCollectSystem] Materialize failed: effective material recipe is not cached for %s",
                GetOwnerName(primitive_comp));
            return false;
        }

        if (getenv("ULRE_ARENA_DEBUG"))
            GLogInfo("[ArenaTrace] materialize entry: material_binding_valid=%d schema_reqs=%u",
                     material_binding_recipe.material_ssbo_binding.IsValid() ? 1 : 0,
                     (uint32_t)material_program->GetShaderResourceSchema().resources.size());

        // Keep the schema-to-recipe readiness check. The recipe binding below is
        // the single source for the BDA row address and active data ID.
        for (const auto &req : material_program->GetShaderResourceSchema().resources)
        {
            if (req.semantic != graph::mtl::DescriptorSemantic::MaterialPrivateData)
                continue;

            const auto &recipe_binding =
                material_binding_recipe.material_ssbo_binding;
            if (!recipe_binding.IsValid()
             || recipe_binding.ssbo_type
                    != ResolveMaterialSSBORequirementType(req))
            {
                GLogWarning("[RenderPrimitiveCollectSystem] Materialize failed: unresolved SSBO binding for %s descriptor=%s type=%s",
                            GetOwnerName(primitive_comp),
                            req.name.empty() ? "<unnamed>" : req.name.c_str(),
                            graph::GetGlobalSSBOTypeName(
                                ResolveMaterialSSBORequirementType(req)));
                return false;
            }
        }

        auto rdbs = world->GetSystem<RenderSceneUBOSystem>();
        if (!rdbs)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] Materialize failed: RenderSceneUBOSystem missing for %s",
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
                static bool arena_trace_done = false;
                if (getenv("ULRE_ARENA_DEBUG") && !arena_trace_done)
                {
                    arena_trace_done = true;
                    GLogInfo("[ArenaTrace] materialize: ssbo_id=%u data_index=%u assets=%u",
                             asset_binding.ssbo_id,
                             asset_binding.data_index,
                             1u);
                }

                row->material_row_cpu = nullptr;
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
                        "[RenderPrimitiveCollectSystem] Materialize failed: material row buffer missing for %s type=%s ssbo_id=%u",
                        GetOwnerName(primitive_comp),
                        graph::GetGlobalSSBOTypeName(
                            asset_binding.ssbo_type),
                        asset_binding.ssbo_id);
                    return false;
                }
                if (!material_domain->IsActive(
                        asset_binding.ssbo_type,
                        asset_binding.data_index))
                {
                    GLogError(
                        "[RenderPrimitiveCollectSystem] Materialize failed: inactive material row ID for %s type=%s ssbo_id=%u data_index=%u",
                        GetOwnerName(primitive_comp),
                        graph::GetGlobalSSBOTypeName(
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
                        "[RenderPrimitiveCollectSystem] Materialize failed: material row buffer invalid for %s type=%s buffer_type=%s ssbo_id=%u data_index=%u capacity=%u row_bytes=%u",
                        GetOwnerName(primitive_comp),
                        graph::GetGlobalSSBOTypeName(
                            asset_binding.ssbo_type),
                        graph::GetGlobalSSBOTypeName(
                            material_buffer.global_ssbo_type),
                        asset_binding.ssbo_id,
                        asset_binding.data_index,
                        material_buffer.row_capacity,
                        material_buffer.row_bytes);
                    return false;
                }

                const uint64_t row_offset =
                    uint64_t(asset_binding.data_index) * material_buffer.row_bytes;
                row->material_row_cpu = material_buffer.cpu_base
                    ? static_cast<uint8_t *>(material_buffer.cpu_base) + row_offset
                    : nullptr;
                row->material_row_gpu = material_buffer.gpu_base + row_offset;

                if (!arena_trace_done)
                {
                    GLogInfo(
                        "[ArenaTrace] translated: gpu=0x%llx (base=%llu row_bytes=%u domain=material)",
                        (unsigned long long)row->material_row_gpu,
                        (unsigned long long)material_buffer.gpu_base,
                        material_buffer.row_bytes);
                }
            }
        }

        // data_index（行号）仍按 data_index VALUE 发布——行表/行尾镜像共用。
        row->data_index_row =
            entity_data_index != uint32_t(-1) ? entity_data_index : 0u;

        auto *texture_graphics_context = world->GetGraphicsContext();
        auto *texture_registry = texture_graphics_context
            ? texture_graphics_context->GetSSBOBufferRegistry()
            : nullptr;

        graph::mtl::MaterialDefinition texture_definition{};
        graph::mtl::MaterialTextureReferenceLayout texture_layout{};
        bool has_texture_definition =
            !effective_recipe.mtl_def_id.empty()
         && graph::mtl::TryGetMaterialDefinitionByID(
                effective_recipe.mtl_def_id,
                texture_definition);
        if (!has_texture_definition)
        {
            has_texture_definition =
                graph::mtl::TryGetMaterialDefinitionByID(
                    graph::mtl::GetFallbackMaterialDefinitionID(),
                    texture_definition);
        }

        if (!has_texture_definition
         || !graph::mtl::BuildMaterialTextureReferenceLayout(
                texture_definition,
                texture_layout))
        {
            GLogWarning(
                "[RenderPrimitiveCollectSystem] Materialize failed: texture definition lookup/layout failed for %s definition=%s",
                GetOwnerName(primitive_comp),
                effective_recipe.mtl_def_id.empty()
                    ? "<empty>" : effective_recipe.mtl_def_id.c_str());
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
                            "[RenderPrimitiveCollectSystem] Required texture binding missing: owner=%s texture=%s",
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
                        "[RenderPrimitiveCollectSystem] Non-array texture received array layer: owner=%s texture=%s layer=%u",
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
                        const MaterialData *material_data =
                            EnsureMaterialDataOf(primitive_comp);
                        const auto *authoring = material_data
                            ? material_data->GetTextureResource(declaration.name)
                            : nullptr;
                        if (authoring
                         && authoring->texture)
                        {
                            const std::string fallback_id =
                                authoring->resource_id.empty()
                                    ? BuildTextureResourceId(authoring->texture)
                                    : authoring->resource_id;
                            handle = rdbs->GetBindlessHandle(
                                AnsiString(fallback_id.c_str()));
                        }
                    }
                }

                if (handle == 0 && declaration.required)
                {
                    GLogError(
                        "[RenderPrimitiveCollectSystem] Required texture handle missing: owner=%s texture=%s resource=%s",
                        GetOwnerName(primitive_comp),
                        declaration.name.c_str(),
                        recipe_binding->resource_id.empty()
                            ? "<direct/empty>"
                            : recipe_binding->resource_id.c_str());
                    return false;
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
                    "[RenderPrimitiveCollectSystem] Materialize failed: SSBO registry missing for texture references owner=%s",
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
                        "[RenderPrimitiveCollectSystem] Materialize failed: texture configuration capacity exhausted owner=%s definition=%s",
                        GetOwnerName(primitive_comp),
                        effective_recipe.mtl_def_id.c_str());
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
                    "[RenderPrimitiveCollectSystem] Materialize failed: texture configuration write failed owner=%s",
                    GetOwnerName(primitive_comp));
                return false;
            }

            if (!can_reuse && old_allocation_live)
                texture_registry->RetireMaterialTextureConfiguration(
                    old_allocation,
                    retire_epoch);

            row->material_texture_configuration = new_allocation;
            row->material_texture_row_cpu =
                new_allocation.cpu_row;
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
                    "[RenderPrimitiveCollectSystem] Materialize failed: texture configuration zero row unavailable owner=%s",
                    GetOwnerName(primitive_comp));
                return false;
            }
            row->material_texture_configuration_hash =
                reference_configuration_hash;

            //if (getenv("ULRE_ARENA_DEBUG"))
            //{
            //    GLogInfo(
            //        "[MaterialTextureReferences] owner=%s definition=%s row=%u references=%u gpu=0x%llx",
            //        GetOwnerName(primitive_comp),
            //        texture_definition.definition_id.c_str(),
            //        new_allocation.row_index,
            //        texture_layout.reference_count,
            //        static_cast<unsigned long long>(
            //            new_allocation.gpu_row));
            //    for (size_t i = 0;
            //         i < texture_definition.texture_declarations.size();
            //         ++i)
            //    {
            //        const auto &declaration =
            //            texture_definition.texture_declarations[i];
            //        const auto &reference =
            //            references[static_cast<int>(i)];
            //        GLogInfo(
            //            "[MaterialTextureReferences] texture=%s descriptor=%u layer=%u",
            //            declaration.name.c_str(),
            //            reference.descriptor_index,
            //            reference.array_layer);
            //    }
            //}
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
            row->material_texture_row_cpu = nullptr;
            row->material_texture_row_gpu = 0;
            row->material_texture_zero_row_gpu = 0;
        }

        slot.runtime_dirty = false;
        slot.valid = false;
        slot.last_materialize_epoch = materialize_epoch;
        return true;
    }

    // D9：阴影 pass 跳过/失败路径的统一收敛入口（见头文件注释）。
    bool RenderPrimitiveCollectSystem::AdvanceShadowRetry(
        MaterialRuntimeSlot &slot,
        const char *reason,
        Entity *primitive_comp)
    {
        // A4：计数归**每实例 slot**（该实例在阴影 pass 上的连续跳过帧数）。
        // A3 曾把它记在共享变体记录上——同键的健康兄弟每帧复位，会持续清零失败者的
        // 计数，掩盖 D9 的 masked 失败告警/降频（只影响诊断，不影响渲染）；迁到每实例
        // 侧后各实例独立收敛。
        //
        // 归属判定与 A3 同口径：只有"该实例已经挂上共享行、且阴影变体已登记"才算有
        // 可记账的位置（解析还没走到算键的地方，例如 graphics 缺失时——那类失败本身
        // 已逐帧有独立告警，见 ResolveShadowCasterProgram）。
        const MaterialRuntimeTable *runtime_table = GetRuntimeTable(world);
        const MaterialRuntimeRow *row = runtime_table ? runtime_table->Get(slot.row) : nullptr;
        if (!row || row->shadow_variant == INVALID_MATERIAL_VARIANT_ID)
            return false;

        const uint32_t retries = ++slot.shadow_retry_frames;
        const char *const name = GetOwnerName(primitive_comp);

        if (retries == 1)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] shadow pass skip for '%s': %s -- "
                        "caster excluded from this frame's depth map; static cascade revision is bumped "
                        "every frame for the first %u frames to converge",
                        name, reason, kShadowRetryFullBumpFrames);
        }
        else if (retries == kShadowRetryFullBumpFrames + 1)
        {
            GLogError("[RenderPrimitiveCollectSystem] shadow pass skip for '%s' persisted %u frames (%s) -- "
                      "throttling static cascade redraw to once every %u frames; this caster's shadow stays "
                      "missing meanwhile (check the forward materialization chain)",
                      name, kShadowRetryFullBumpFrames, reason, kShadowRetryBumpPeriod);
        }

        return retries <= kShadowRetryFullBumpFrames
            || (retries % kShadowRetryBumpPeriod) == 0;
    }

    void RenderPrimitiveCollectSystem::Update(float /*deltaTime*/)
    {
        if (!world)
            return;

        // 本 pass 生效相机的 CameraInfo（离屏 pass = pass 相机，主帧 = 主相机）：
        // 每帧现取，**不缓存指针**——pass 之间相机会换（旧实现把指针缓存在安装期，pass 相机一上场就错）。
        const graph::CameraInfo *cameraInfo = world->GetActiveCameraInfo();

        if (!cameraInfo)
            return;

        auto& cache = world->GetRenderFrameCache();
        cache.cameraInfo = cameraInfo;
        cache.BeginFrame();

        // A7b：本 pass 回退（错误）材质配方合成次数（诊断；默认 0 ⇒ 不打印任何东西，
        // 基线日志逐字不变）。
        g_fallback_recipe_builds = 0;

        const int active_mobility_filter = world ? world->GetActiveMobilityFilter() : -1;

        std::vector<std::shared_ptr<GeometryData>> primitives;
        world->GetComponents<GeometryData>(primitives);

        // A4：材质运行期表（世界私有）——共享行 = 可共享的材质绑定；slot = 每实例状态。
        MaterialRuntimeTable *runtime_table = world->GetMaterialRuntimeTable();

        // P1-1: Global frame-level materialize gating.
        //
        // PrepareActivePlanResources and MaterializeRecipeRowsForPrimitive are
        // the per-frame hotspot: they re-traverse the resource plan, rebuild the
        // binding recipe and re-materialize rows on every Update even when
        // nothing changed. This pre-scan mirrors the resolve fast-path
        // (ResolveMaterialProgramForPrimitive), the materialization epoch and
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

        for (const auto& geometryComp : primitives)
        {
            if (!geometryComp)
                continue;

            // A7a：收集判定 = 策略表（唯一判据）；事实由组件 / 世界回答。mobility 过滤是
            // 世界级过滤器（表不建模），仍在本循环里单独生效。
            const CollectStrategy strategy = EvaluateCollectStrategy(world,geometryComp.get());

#if ULRE_STRATEGY_PARITY_ENABLED
            // ── 反向守卫：参考实现 = 手写 if 链（Release 下整块编空）───────────
            // 只有"表结论 = 参考实现结果"被每帧同输入实测过，表才算真接管了判据。
            // A7b：参考实现**已按新语义**去掉“有材质来源”这一条 —— 该条被用户拍板从
            // “收集判据”改为“材质错误（走回退材质）”，故它不再参与收集对拍，而是由下面
            // 独立的 ParityCheckFallbackMaterial 逐帧对拍。这不是静默放水：把无来源改回
            // 剔除、或把回退规则写歪，都会在那条守卫上立刻报不一致。
            {
                const bool legacy_collect =
                       world->IsEntityVisible(geometryComp->GetOwnerID())
                    && CanRender(geometryComp->GetOwner())
                    && (geometryComp->GetOwner() != nullptr)
                    && (!strategy.facts.shadow_pass
                        || (CanCastShadow(geometryComp->GetOwner()) && strategy.in_shadow_range));

                ParityCheckCollect(HasRenderNeed(strategy.needs,RenderNeed::CollectForCurrentPass),
                                   legacy_collect,
                                   "RenderPrimitiveCollectSystem#prescan");

                ParityCheckShadowCaster(HasRenderNeed(strategy.needs,RenderNeed::ShadowCaster),
                                        CanCastShadow(geometryComp->GetOwner()),
                                        "RenderPrimitiveCollectSystem#prescan");

                // A7b 新语义守卫：表说“材质缺失 ⇒ 回退材质” ⟺ 事实“无任何材质来源”。
                ParityCheckFallbackMaterial(HasRenderNeed(strategy.needs,RenderNeed::FallbackMaterial),
                                            !HasAnyMaterialSource(geometryComp->GetOwner()),
                                            "RenderPrimitiveCollectSystem#prescan");
            }
#endif

            if (!HasRenderNeed(strategy.needs,RenderNeed::CollectForCurrentPass))
                continue;

            // 实体级可见性与 owner 已由表的 CollectForCurrentPass 要求 ⇒ 不再单独判据
            Entity* entity = geometryComp->GetOwner();

            if (active_mobility_filter >= 0)
            {
                TransformAccessor transform = world->GetTransformByEntity(entity->GetEntityID());
                if (!transform.IsValid() || static_cast<int>(transform.GetMobility()) != active_mobility_filter)
                    continue;
            }

            // 阴影 pass 的 caster 能力（CanCastShadow）与距离裁剪（in_shadow_range）已由表的
            // CollectForCurrentPass 条件要求表达 ⇒ 这里不再重复判据。

            // A4：材质运行期状态 = **每实例 slot**（世界表里按实体稀疏存放）+
            // **共享行**（绑定）。这里取/建 slot，再经行取 program 与缓存哈希。
            MaterialRuntimeSlot &material_slot =
                world->GetOrCreateMaterialRuntimeSlot(entity);
            const MaterialRuntimeRow *material_row = runtime_table
                ? runtime_table->Get(material_slot.row)
                : nullptr;

            const graph::ShaderProgram *forward_program =
                GetForwardProgram(GetVariantTable(world), material_row);

            const bool runtime_rows =
                forward_program
             && graph::mtl::MaterialRequiresRecipeRuntimeRows(
                    forward_program->GetShaderResourceSchema());

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
                   == MaterialAuthoredGenerationOf(geometryComp->GetOwner())
                && material_row
                && material_row->cached_effective_recipe_hash != 0;

            const bool epoch_stale =
                runtime_rows
             && material_slot.last_materialize_epoch != materialize_epoch;

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

        if (any_material_work && any_possible_runtime_rows_visible)
            ++materialize_epoch;

        size_t skipped_invisible = 0;
        size_t skipped_no_owner = 0;
        size_t skipped_no_transform = 0;
        // S6 诊断：本 pass 产出图元的实体 ID 校验和（判定"条带帧 vs 整级帧是否同一批 caster"）
        uint64_t shadow_pass_idsum = 0;
        size_t added = 0;

        const glm::vec3 camera_pos = glm::vec3(cameraInfo->pos);

        for (const auto& geometryComp : primitives)
        {
            if (!geometryComp)
                continue;

            // A7a：收集判定 = 策略表（唯一判据）；事实由组件 / 世界回答。
            const CollectStrategy strategy = EvaluateCollectStrategy(world,geometryComp.get());

#if ULRE_STRATEGY_PARITY_ENABLED
            // ── 反向守卫：参考实现 = 手写 if 链（Release 下整块编空）───────────
            // 只有"表结论 = 参考实现结果"被每帧同输入实测过，表才算真接管了判据。
            // A7b：参考实现**已按新语义**去掉“有材质来源”这一条 —— 该条被用户拍板从
            // “收集判据”改为“材质错误（走回退材质）”，故它不再参与收集对拍，而是由下面
            // 独立的 ParityCheckFallbackMaterial 逐帧对拍。这不是静默放水：把无来源改回
            // 剔除、或把回退规则写歪，都会在那条守卫上立刻报不一致。
            {
                const bool legacy_collect =
                       world->IsEntityVisible(geometryComp->GetOwnerID())
                    && CanRender(geometryComp->GetOwner())
                    && (geometryComp->GetOwner() != nullptr)
                    && (!strategy.facts.shadow_pass
                        || (CanCastShadow(geometryComp->GetOwner()) && strategy.in_shadow_range));

                ParityCheckCollect(HasRenderNeed(strategy.needs,RenderNeed::CollectForCurrentPass),
                                   legacy_collect,
                                   "RenderPrimitiveCollectSystem#collect");

                ParityCheckShadowCaster(HasRenderNeed(strategy.needs,RenderNeed::ShadowCaster),
                                        CanCastShadow(geometryComp->GetOwner()),
                                        "RenderPrimitiveCollectSystem#collect");

                // A7b 新语义守卫：表说“材质缺失 ⇒ 回退材质” ⟺ 事实“无任何材质来源”。
                ParityCheckFallbackMaterial(HasRenderNeed(strategy.needs,RenderNeed::FallbackMaterial),
                                            !HasAnyMaterialSource(geometryComp->GetOwner()),
                                            "RenderPrimitiveCollectSystem#collect");
            }
#endif

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
                        ++skipped_invisible;
                    else if (!strategy.facts.has_owner)
                        ++skipped_no_owner;
                }

                continue;
            }

            EntityID entity_id = geometryComp->GetOwnerID();

            // 实体级可见性与 owner 已由表的 CollectForCurrentPass 要求 ⇒ 不再单独判据
            Entity* entity = geometryComp->GetOwner();

            TransformAccessor transform = world->GetTransformByEntity(entity->GetEntityID());

            if (!transform.IsValid())
            {
                ++skipped_no_transform;
                continue;
            }

            if (active_mobility_filter >= 0 && static_cast<int>(transform.GetMobility()) != active_mobility_filter)
            {
                continue;
            }

            // 阴影 pass 的 caster 能力与距离裁剪已由表的 CollectForCurrentPass 条件要求表达
            // ⇒ 这里不再重复判据（原两条 continue 随之删除）。

            // A4：本实例的材质运行期 slot（**有材质来源**已由表的 CollectForCurrentPass 蕴含
            // ⇒ 不再需要"有 / 无来源"双分支；保留一层作用域标出块边界，与预扫描同一份 slot）。
            MaterialRuntimeSlot *material_slot_ptr = nullptr;

            {
                // A4：本实例的材质运行期 **slot**（世界表按实体稀疏存放；与预扫描同一份）。
                MaterialRuntimeSlot &material_slot =
                    world->GetOrCreateMaterialRuntimeSlot(entity);
                material_slot_ptr = &material_slot;

                if (!ResolveMaterialProgramForPrimitive(geometryComp->GetOwner(), material_slot))
                {
                    if (world->IsCurrentPassShadow())
                    {
                        // A1-2：阴影解析失败只清阴影槽。InvalidateRecipeRuntime
                        // 的 retire 纹理配置/清物化行/置 program_dirty 全是
                        // forward 槽语义，由阴影失败触发会把 forward 链整链
                        // 拖垮（持续失败时每帧 retire+重建）。清槽后下个阴影
                        // 帧快路径自然失配并重试。
                        if (const MaterialRuntimeRow *row = runtime_table
                                ? runtime_table->Get(material_slot.row)
                                : nullptr)
                        {
                            ClearVariantProgram(GetVariantTable(world),
                                                row->shadow_variant);
                        }
                        // 固化防御：本帧深度图缺了这个 caster，静态级联若全量
                        // 重绘过就会把"无它"的内容缓存住。借 A3 revision 链让
                        // EnvironmentSystem 下帧失效重画，直至 resolve 成功。
                        // D9：告警与降频都经统一收敛入口——阴影 pass 只在此处
                        // 输出一条（每 episode），不再外面逐帧刷屏。
                        if (AdvanceShadowRetry(material_slot,
                                               "shadow caster program resolve failed",
                                               geometryComp->GetOwner()))
                            world->BumpStaticSceneRevision();
                    }
                    else
                    {
                        GLogWarning(
                            "[RenderPrimitiveCollectSystem] ResolveMaterialProgramForPrimitive failed for %s",
                            GetOwnerName(geometryComp->GetOwner()));
                        // A4：行键不变（解析没走到算键处）⇒ 就地把本行物化绑定作废 + 退 program。
                        InvalidateRecipeRuntime(world, material_slot, true, true);
                        material_slot.valid = false;        // 等价于 MarkFailed()
                    }
                }
                else if (world->IsCurrentPassShadow())
                {
                    // A1：阴影 pass 精简链。ShadowCaster 程序不消费材质 SSBO
                    // payload；几何+管线即可绘制。
                    //
                    // A1-4：masked caster（ShadowCasterMasked 模板）的片元要
                    // 采样 opacity mask → 需要纹理引用行。行由 **主帧 forward
                    // 链** 物化并持有（行内容随授权态、与 program 无关，
                    // shadow program 共读）。这里**绝不**在阴影帧代为物化：
                    // 阴影帧与主帧两条物化链会互相 retire/重分配纹理配置行，
                    // 深度图采样引用的池行随即漂移 → opacity 槽失效 → 影子
                    // 退化为实心。行未就绪（valid==false，如首帧 prepass 早于
                    // 任何主帧物化）时跳过本帧该 caster，并 bump static_scene_
                    // revision 触发静态级联下帧重画，直至行就绪（收敛）。
                    const MaterialRuntimeRow *shadow_row = runtime_table
                        ? runtime_table->Get(material_slot.row)
                        : nullptr;
                    const graph::ShaderProgram *shadow_pass_program =
                        GetShadowProgram(GetVariantTable(world), shadow_row);

                    const bool shadow_needs_rows =
                        shadow_pass_program
                     && graph::mtl::MaterialRequiresRecipeRuntimeRows(
                            shadow_pass_program->GetShaderResourceSchema());

                    if (shadow_needs_rows && !material_slot.valid)
                    {
                        // D9：本路径原本完全静默（只 bump+continue）。原因经统一收敛
                        // 入口记录：首次跳过告警一次；连续超过 kShadowRetryFullBumpFrames
                        // 帧后报错并把 bump 降频——否则"每帧 bump → 静态级联每帧全量
                        // 重画"会持续到场景结束且无任何日志。
                        if (AdvanceShadowRetry(
                                material_slot,
                                "masked caster runtime rows not ready (forward chain has not materialized them)",
                                geometryComp->GetOwner()))
                            world->BumpStaticSceneRevision();
                        continue; // 本帧深度图不含它；下帧行就绪后重画
                    }

                    if (!EnsureRuntimeGeometryFromAsset(
                            world, geometryComp->GetOwner(), material_slot, shadow_row))
                    {
                        if (shadow_row)
                            ClearVariantProgram(GetVariantTable(world),
                                                shadow_row->shadow_variant);
                        // D9：告警与 bump 都经统一收敛入口（不再逐帧刷屏）
                        if (AdvanceShadowRetry(material_slot,
                                               "shadow pass geometry failed",
                                               geometryComp->GetOwner()))
                            world->BumpStaticSceneRevision(); // 固化防御（同上）
                    }
                    else if (!ResolveRuntimePipelineForPrimitive(
                                 geometryComp->GetOwner(), material_slot))
                    {
                        if (shadow_row)
                            ClearVariantProgram(GetVariantTable(world),
                                                shadow_row->shadow_variant);
                        // D9：同上
                        if (AdvanceShadowRetry(material_slot,
                                               "shadow pass pipeline failed",
                                               geometryComp->GetOwner()))
                            world->BumpStaticSceneRevision(); // 固化防御（同上）
                    }
                }
                else if (!any_material_work
                         && material_slot.last_materialize_epoch == materialize_epoch)
                {
                    // P1-1: all-clean frame — no primitive requires
                    // materialization work, this primitive's full chain
                    // succeeded last frame (valid), and the current
                    // materialization epoch is already covered. Everything
                    // cached (resource preparation, materialization rows,
                    // runtime geometry/pipeline) is still valid, so skip the
                    // expensive re-prepare / re-materialize chain. Only the
                    // cheap resolve fast-paths run; on any unexpected failure
                    // fall back to MarkFailed so the next frame retries the
                    // full chain.
                    const bool chain_ok =
                        ResolveMaterialProgramForPrimitive(
                            geometryComp->GetOwner(), material_slot)
                     && EnsureRuntimeGeometryFromAsset(
                            world, geometryComp->GetOwner(), material_slot,
                            runtime_table ? runtime_table->Get(material_slot.row) : nullptr)
                     && ResolveRuntimePipelineForPrimitive(
                            geometryComp->GetOwner(), material_slot);
                    if (chain_ok)
                        material_slot.valid = true;     // 等价于 MarkValid()
                    else
                        material_slot.valid = false;    // 等价于 MarkFailed()
                }
                else
                {
                    const MaterialRuntimeRow *forward_row = runtime_table
                        ? runtime_table->Get(material_slot.row)
                        : nullptr;
                    graph::ShaderProgram *forward_program =
                        GetForwardProgram(GetVariantTable(world), forward_row);

                    material_slot.valid = false;        // 等价于 MarkResourcesPending()
                    const bool resources_ready =
                        PrepareActivePlanResources(
                            world,
                            geometryComp->GetOwner(),
                            forward_program,
                            forward_row
                                ? forward_row->cached_effective_recipe
                                : graph::mtl::MaterialRecipe{});
                    if (!resources_ready)
                    {
                        GLogWarning(
                            "[RenderPrimitiveCollectSystem] Material resources failed for %s program=%s",
                            GetOwnerName(geometryComp->GetOwner()),
                            forward_program
                                ? forward_program->
                                    GetName().c_str()
                                : "<null>");
                        InvalidateRecipeRuntime(world, material_slot, true, false);
                        material_slot.valid = false;
                    }
                    else if (!MaterializeRecipeRowsForPrimitive(
                                geometryComp->GetOwner(), material_slot))
                    {
                        GLogWarning(
                            "[RenderPrimitiveCollectSystem] MaterializeRecipeRowsForPrimitive failed for %s program=%s",
                            GetOwnerName(geometryComp->GetOwner()),
                            forward_program
                                ? forward_program->
                                    GetName().c_str()
                                : "<null>");
                        InvalidateRecipeRuntime(world, material_slot, true, false);
                        material_slot.valid = false;
                    }
                    else if (!EnsureRuntimeGeometryFromAsset(
                                world, geometryComp->GetOwner(), material_slot,
                                runtime_table ? runtime_table->Get(material_slot.row) : nullptr))
                    {
                        GLogWarning(
                            "[RenderPrimitiveCollectSystem] EnsureRuntimeGeometryFromAsset failed for %s",
                            GetOwnerName(geometryComp->GetOwner()));
                        material_slot.valid = false;
                    }
                    else if (!ResolveRuntimePipelineForPrimitive(
                                geometryComp->GetOwner(), material_slot))
                    {
                        GLogWarning(
                            "[RenderPrimitiveCollectSystem] ResolveRuntimePipelineForPrimitive failed for %s",
                            GetOwnerName(geometryComp->GetOwner()));
                        material_slot.valid = false;
                    }
                    else
                    {
                        material_slot.valid = true;     // 等价于 MarkValid()
                        GLogVerbose(
                            "[DeferredResource] owner=%s valid=%d",
                            GetOwnerName(geometryComp->GetOwner()),
                            material_slot.valid ? 1 : 0);
                    }
                }
            }


            // A4：解析/物化可能刚换过行 ⇒ 重新按**行号**取指针（不复用上面任何行指针）。
            const MaterialRuntimeRow *material_for_item = (runtime_table && material_slot_ptr)
                ? runtime_table->Get(material_slot_ptr->row)
                : nullptr;

            // ── 同步 4-ID 描述符至 RenderItemDataStorage（句柄存每实例 slot）──
            const uint32_t transform_id = transform.GetID();
            uint32_t geometry_id = 0;
            GeometryData *geometry_comp = geometryComp.get();
            const auto *geom_buf = geometry_comp ? geometry_comp->GetRuntimeGeometryDataBuffer() : nullptr;
            if (geom_buf)
            {
                geometry_id = geom_buf->geometry_id;
            }
            if (geometry_id == 0 && geometry_comp && geometry_comp->GetPrimitiveAsset())
            {
                if (auto *geom = geometry_comp->GetPrimitiveAsset()->GetGeometry())
                {
                    auto *gc = world ? world->GetGraphicsContext() : nullptr;
                    auto *pool = gc ? gc->GetGlobalSSBOBufferRegistry() : nullptr;
                    auto *dev = world ? world->GetGPUDevice() : nullptr;
                    if (pool && dev)
                    {
                        const_cast<graph::Geometry *>(geom)->EnsureMeshDrawParams(pool, dev);
                    }
                    geometry_id = geom->GetGeometryID();
                    if (geom_buf)
                    {
                        const_cast<graph::GeometryDataBuffer *>(geom_buf)->geometry_id = geometry_id;
                    }
                }
            }
            const uint32_t material_id = (material_for_item && material_for_item->data_index_row != uint32_t(-1))
                ? material_for_item->data_index_row : 0;
            const uint32_t texture_id = (material_for_item && material_for_item->material_texture_configuration.IsValid())
                ? material_for_item->material_texture_configuration.row_index : 0;

            // A4：RenderItem 只持**行号**（绑定状态在世界表里；行由本实体 slot 持引用，
            // collect → batch 之间不会失效）。
            const MaterialRuntimeRowID material_row_id =
                material_for_item ? material_slot_ptr->row : INVALID_MATERIAL_RUNTIME_ROW_ID;

            std::unique_ptr<PrimitiveRenderItem> item;

            // A5b：多实例与否按**每实例 slot** 判定（原图元多实例组件已删）：
            // 分配过连续槽位 / GPU 驱动 / 间接 = 多实例绑定。
            const MaterialRuntimeSlot *render_slot = runtime_table
                ? runtime_table->GetSlot(entity_id)
                : nullptr;
            const bool is_instanced = render_slot
                && (render_slot->allocated_instance_capacity > 0
                 || render_slot->is_gpu_driven
                 || render_slot->is_indirect);

            if (is_instanced && render_slot->allocated_instance_capacity > 1)
            {
                SetAllInstances4ID(*world, entity_id, transform_id, geometry_id,
                                   material_id, texture_id, false);
            }
            else
            {
                SetRenderItem4ID(*world, entity_id, transform_id, geometry_id,
                                 material_id, texture_id);
            }

            if (is_instanced)
            {
                item = std::make_unique<InstancedPrimitiveRenderItem>(
                    entity_id, transform, material_row_id, world);
            }
            else
            {
                item = std::make_unique<PrimitiveRenderItem>(
                    entity_id, transform, material_row_id, world);
            }

            const glm::vec3 worldPos = transform.GetWorldPosition();
            item->distanceToCamera = glm::length(worldPos - camera_pos);

            item->UpdateWorldMatrix();

            // D9：阴影 pass 的 caster 本帧成功产出 item（该实例的阴影变体已就绪）
            // ⇒ 影子链已恢复正常，复位**该实例**的重试计数（A4 起计数归每实例 slot：
            // 同键的健康兄弟不再每帧清零失败者的计数，D9 的告警/降频对每个实例独立）。
            if (material_slot_ptr && world->IsCurrentPassShadow())
            {
                const MaterialVariantTable *variant_table = GetVariantTable(world);
                const MaterialVariantRecord *shadow_record = variant_table
                    ? variant_table->Get(material_for_item ? material_for_item->shadow_variant
                                                           : INVALID_MATERIAL_VARIANT_ID)
                    : nullptr;

                if (shadow_record && shadow_record->program)
                    material_slot_ptr->shadow_retry_frames = 0;
            }


            cache.renderItems.push_back(std::move(item));
            cache.renderableCount++;
            ++added;
            shadow_pass_idsum += (static_cast<uint64_t>(entity_id.index) << 16) | entity_id.generation;
        }

        // S6 诊断（CSM_PASS_LOG=1 打开，默认静默）：阴影 pass 的收集结论。
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

        static const bool s6_collect_log = (std::getenv("CSM_PASS_LOG") != nullptr);
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
    }
}//namespace hgl::ecs
