#include <hgl/ecs/systems/render/collect/RenderCollectSystem.h>
#include "RenderCollectInternal.h"
#include <hgl/ecs/support/pipeline/RenderResource.h>
#include <hgl/ecs/support/material/MaterialVariantTable.h>
#include <hgl/ecs/systems/render/buffer/RenderSceneUBOSystem.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/ShaderProgramManager.h>
#include <hgl/graph/module/SSBOBufferRegistry.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/graph/render/RenderContext.h>
#include <hgl/mtl/MaterialResolver.h>
#include <hgl/common/DebugChannel.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    using namespace hgl::ecs::detail::render_collect;

    namespace
    {
        const graph::mtl::RecipeTextureBinding *FindRecipeTextureBinding(
            const graph::mtl::MaterialRecipe &recipe,
            const auto &texture_name) noexcept
        {
            for (const auto &binding : recipe.textures)
            {
                if (binding.texture_name == texture_name)
                    return &binding;
            }
            return nullptr;
        }

    }

    bool detail::render_collect::ValidateMaterialRecipeForRuntime(
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

    namespace
    {
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

                const auto resource_id =
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

        const MaterialVariantTable *variants = world->GetMaterialVariantTable();
        const MaterialVariantRecord *variant = variants ? variants->Get(row->forward_variant) : nullptr;
        graph::ShaderProgram *material_program = variant ? variant->program : nullptr;

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
                            const auto fallback_id =
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


}
