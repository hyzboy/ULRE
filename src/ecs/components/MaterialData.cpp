#include<hgl/ecs/components/MaterialData.h>
#include<hgl/ecs/support/RenderResource.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    namespace
    {
        void ResetRecipe(hgl::graph::mtl::MaterialRecipe &recipe)
        {
            recipe.recipe_name.clear();
            recipe.mtl_def_id.clear();
            recipe.vertex_node_config = hgl::graph::mtl::MakeDefault3DNodeConfig();
            recipe.render_state_overrides.has_double_sided = true;
            recipe.render_state_overrides.double_sided = false;
            recipe.render_state_overrides.has_alpha_test = true;
            recipe.render_state_overrides.alpha_test = false;
            recipe.render_state_overrides.has_alpha_cutoff = true;
            recipe.render_state_overrides.alpha_cutoff = 0.5f;
            recipe.render_state_overrides.has_dither = true;
            recipe.render_state_overrides.dither = false;
            recipe.render_state_overrides.has_pipeline_config = true;
            recipe.render_state_overrides.pipeline_config = hgl::graph::mtl::MaterialPipelineConfig{};
            recipe.textures.clear();
            recipe.material_ssbo_binding = {};
        }

        bool UpsertTextureAuthoringResource(
            hgl::UnorderedMap<hgl::AnsiString,
                MaterialData::MaterialTextureAuthoringResource> &resources,
            const std::string &name,
            const MaterialData::MaterialTextureAuthoringResource &resource)
        {
            const hgl::AnsiString key(name.c_str());
            if (resources.GetValuePointer(key))
                return resources.Change(key, resource);
            return resources.Add(key, resource);
        }

        bool AppendTextureBinding(
            hgl::graph::mtl::MaterialRecipe &recipe,
            const std::string &name,
            const MaterialData::MaterialTextureAuthoringResource &resource)
        {
            if (!resource.texture || !resource.sampler)
                return true;

            const std::string resource_id = resource.resource_id.empty()
                ? BuildTextureResourceId(resource.texture)
                : resource.resource_id;
            if (resource_id.empty())
                return false;

            return hgl::graph::mtl::UpsertRecipeTextureBinding(
                recipe,
                name,
                resource_id,
                resource.required,
                resource.array_layer);
        }
    }

    void MaterialData::SetRecipe(const hgl::graph::mtl::MaterialRecipe &recipe)
    {
        recipe_override = recipe;
        has_recipe_override = true;
        TouchAuthoredRow(hgl::graph::ssbo::kMaterialDataFlagAuthoredRecipe, 0);
    }

    const hgl::graph::mtl::MaterialRecipe *MaterialData::GetRecipeOverride() const
    {
        if (has_recipe_override)
            return &recipe_override;

        return nullptr;
    }

    bool MaterialData::SetTextureResource(
        const std::string &name,
        const MaterialTextureAuthoringResource &resource)
    {
        if (!hgl::graph::mtl::IsValidMaterialTextureName(name))
        {
            GLogError(
                "[MaterialData] Texture authoring rejected invalid name=%s",
                name.c_str());
            return false;
        }
        if (!resource.texture || !resource.sampler)
        {
            GLogError(
                "[MaterialData] Texture authoring rejected null resource name=%s",
                name.c_str());
            return false;
        }

        if (!UpsertTextureAuthoringResource(
                named_texture_resources,
                name,
                resource))
        {
            GLogError(
                "[MaterialData] Texture authoring rejected upsert failed name=%s",
                name.c_str());
            return false;
        }

        TouchAuthoredRow(hgl::graph::ssbo::kMaterialDataFlagAuthoredTexture, 0);
        return true;
    }

    bool MaterialData::SetTextureResource(
        const std::string &name,
        hgl::graph::Texture *texture,
        hgl::graph::Sampler *sampler,
        MaterialTextureResourceKind kind,
        const std::string &resource_id,
        const uint32_t array_layer,
        const bool required)
    {
        MaterialTextureAuthoringResource resource{};
        resource.resource_id = resource_id.empty()
            ? BuildTextureResourceId(texture)
            : resource_id;
        resource.texture       = texture;
        resource.sampler       = sampler;
        resource.kind          = kind;
        resource.array_layer   = array_layer;
        resource.required      = required;

        return SetTextureResource(name, resource);
    }

    const MaterialData::MaterialTextureAuthoringResource *
        MaterialData::GetTextureResource(
            const std::string &name) const
    {
        if (!hgl::graph::mtl::IsValidMaterialTextureName(name))
            return nullptr;

        const hgl::AnsiString key(name.c_str());
        if (const auto *entry = named_texture_resources.GetValuePointer(key))
        {
            if (entry->texture && entry->sampler)
                return entry;
        }
        return nullptr;
    }

    // ── B4-iii-a：材质数据在设备级的家 = (Global, MaterialData) 组件行 ──────────
    //   真源只有一处：**行**。组件只持行号 ⇒ 不存在「组件副本 vs 行」双真值。
    //   行归属设备级（材质数据是资源级、跨世界共享）—— 与 P-1 及 B4 拍板「路 B」一致。

    hgl::graph::GlobalSSBOBufferRegistry *MaterialData::GetRegistry() const
    {
        const Entity *owner = GetOwner();
        ECSContext *world = owner ? owner->GetContext() : nullptr;
        hgl::graph::GraphicsContext *gc = world ? world->GetGraphicsContext() : nullptr;

        return gc ? gc->GetGlobalSSBOBufferRegistry() : nullptr;
    }

    bool MaterialData::EnsureMaterialRow()
    {
        if (material_row_id != 0)
            return true;

        auto *registry = GetRegistry();

        if (!registry)
        {
            GLogError("[MaterialData] 无设备/注册表：无法分配 (Global, MaterialData) 组件行 owner_id=%llu",
                      GetOwnerID().index);
            return false;
        }

        const uint32_t row = registry->Acquire(hgl::graph::GlobalSSBOType::MaterialData);

        if (row == 0 || row == hgl::graph::ActiveRowPool::InvalidRowID)
        {
            GLogError("[MaterialData] (Global, MaterialData) 行池分配失败（预算用尽？）owner_id=%llu",
                      GetOwnerID().index);
            return false;
        }

        material_row_id = row;

        // 一次性证据（B3 同款「就位」留痕）：设备级行 arena 真的被用上
        static bool s_logged = false;
        if (!s_logged)
        {
            s_logged = true;
            GLogInfo("[MaterialData] (Global, MaterialData) 组件行就位：row=%u（设备级、跨世界共享；行 0 = 无材质数据哨兵）",
                     row);
        }

        return true;
    }

    const hgl::graph::ssbo::MaterialDataRow *MaterialData::GetMaterialRow() const
    {
        if (material_row_id == 0)
            return nullptr;

        auto *registry = GetRegistry();

        return registry ? registry->GetMaterialDataRow(material_row_id) : nullptr;
    }

    bool MaterialData::WriteMaterialRow(const hgl::graph::ssbo::MaterialDataRow &row)
    {
        auto *registry = GetRegistry();

        return registry
            && registry->Write(hgl::graph::GlobalSSBOType::MaterialData, material_row_id, row);
    }

    void MaterialData::ClearMaterialRow()
    {
        const hgl::graph::ssbo::MaterialDataRow cleared{};

        if (!WriteMaterialRow(cleared))
            GLogError("[MaterialData] 清空 (Global, MaterialData) 行失败 row=%u", material_row_id);
    }

    void MaterialData::SetDataResource(
        const MaterialDataAuthoringResource &resource)
    {
        if (resource.ssbo_id == 0)
        {
            // 清空路径：行保留（行号不还），只把内容复位 ⇒ 下游按「无数据行」走。
            if (material_row_id != 0)
            {
                ClearMaterialRow();
                TouchAuthoredRow(0, hgl::graph::ssbo::kMaterialDataFlagAuthoredDataResource);
            }
            return;
        }

        if (!resource.GetGlobalSSBOBinding().IsValid())
        {
            GLogError(
                "[MaterialData] Material data resource rejected missing active row ID type=%s ssbo_id=%u data_index=%u",
                hgl::graph::GetGlobalSSBOTypeName(
                    resource.ssbo_type),
                resource.ssbo_id,
                resource.data_index);
            return;
        }

        if (!EnsureMaterialRow())
            return;

        const hgl::graph::GlobalSSBOBinding binding = resource.GetGlobalSSBOBinding();

        hgl::graph::ssbo::MaterialDataRow row{};
        row.surface_type       = static_cast<uint32>(binding.ssbo_type);
        row.surface_ssbo_id    = binding.ssbo_id;
        row.surface_data_index = binding.data_index;
        row.authored_flags     = hgl::graph::ssbo::PackMaterialDataAuthoredFlags(
            hgl::graph::ssbo::kMaterialDataFlagAuthoredDataResource,
            ++authored_generation);

        if (!WriteMaterialRow(row))
        {
            GLogError("[MaterialData] 写 (Global, MaterialData) 行失败 row=%u ssbo_id=%u data_index=%u",
                      material_row_id, binding.ssbo_id, binding.data_index);
            return;
        }
    }

    void MaterialData::ClearAuthoredResources()
    {
        named_texture_resources.Clear();

        if (material_row_id != 0)
            ClearMaterialRow();

        TouchAuthoredRow(0, hgl::graph::ssbo::kMaterialDataFlagMask);
    }

    // B4-ii：授权变更唯一入口（见 .h 注释）。行内 revision 与授权位**只在这里落笔**。
    void MaterialData::TouchAuthoredRow(const uint32_t set_flags, const uint32_t clear_flags)
    {
        ++authored_generation;

        if (material_row_id == 0 && !EnsureMaterialRow())
            return;                     // 无设备/预算用尽：代数已前进，行内副本留待下次

        auto *registry = GetRegistry();

        if (!registry)
            return;

        const auto *current = registry->GetMaterialDataRow(material_row_id);

        hgl::graph::ssbo::MaterialDataRow row =
            current ? *current : hgl::graph::ssbo::MaterialDataRow{};

        const uint32_t flags = (hgl::graph::ssbo::GetMaterialDataRowFlags(row) | set_flags)
                             & ~clear_flags & hgl::graph::ssbo::kMaterialDataFlagMask;

        row.authored_flags = hgl::graph::ssbo::PackMaterialDataAuthoredFlags(
            flags, authored_generation);

        if (!registry->Write(hgl::graph::GlobalSSBOType::MaterialData, material_row_id, row))
            GLogError("[MaterialData] 写组件行（授权位 0x%X + revision %u）失败 row=%u",
                      flags, authored_generation, material_row_id);
    }

    uint32_t MaterialData::GetAuthoredRowRevision() const
    {
        const auto *row = GetMaterialRow();

        return row ? hgl::graph::ssbo::GetMaterialDataRowRevision(*row) : authored_generation;
    }

    bool MaterialData::BuildResolvedRecipe(hgl::graph::mtl::MaterialRecipe &out_recipe,
                                           const hgl::graph::ShaderProgram *material_program,
                                           const hgl::graph::mtl::MaterialRecipe *asset_default_recipe) const
    {
        (void)material_program;

        const hgl::graph::mtl::MaterialRecipe *override_recipe = GetRecipeOverride();

        if (asset_default_recipe)
            out_recipe = *asset_default_recipe;
        else
        if (override_recipe)
            out_recipe = *override_recipe;
        else
            return false;

        if (asset_default_recipe && override_recipe)
            out_recipe = *override_recipe;

        hgl::graph::mtl::MaterialDefinition definition{};
        const bool has_definition =
            !out_recipe.mtl_def_id.empty()
         && hgl::graph::mtl::TryGetMaterialDefinitionByID(
                out_recipe.mtl_def_id,
                definition);

        for (const auto &[named_key, resource] : named_texture_resources)
        {
            const std::string name = named_key.c_str();
            if (!hgl::graph::mtl::IsValidMaterialTextureName(name))
                return false;

            const int declaration_index = has_definition
                ? hgl::graph::mtl::FindMaterialTextureDeclaration(
                    definition,
                    name)
                : -1;
            if (!has_definition || declaration_index < 0)
                return false;

            if (declaration_index >= 0)
            {
                const auto &declaration =
                    definition.texture_declarations[
                        static_cast<size_t>(declaration_index)];
                const bool declaration_uses_array =
                    hgl::graph::mtl::IsMaterialTextureArraySampler(
                        declaration.sampler_type);
                const bool authoring_uses_array =
                    resource.kind
                    == MaterialTextureResourceKind::Texture2DArray;
                if ((!declaration_uses_array && authoring_uses_array)
                 || (!authoring_uses_array && resource.array_layer != 0))
                    return false;
            }
        }

        if (has_definition)
        {
            for (const auto &declaration : definition.texture_declarations)
            {
                const hgl::AnsiString key(declaration.name.c_str());
                const auto *resource =
                    named_texture_resources.GetValuePointer(key);
                if (resource
                 && !AppendTextureBinding(
                        out_recipe,
                        declaration.name,
                        *resource))
                    return false;

                bool has_recipe_binding = false;
                for (const auto &binding : out_recipe.textures)
                {
                    if (binding.texture_name == declaration.name)
                    {
                        has_recipe_binding = true;
                        if (!hgl::graph::mtl::IsMaterialTextureArraySampler(
                                declaration.sampler_type)
                         && binding.array_layer != 0)
                            return false;
                        break;
                    }
                }
                if (!has_recipe_binding
                 && !hgl::graph::mtl::UpsertRecipeTextureBinding(
                        out_recipe,
                        declaration.name,
                        std::string(),
                        declaration.required))
                    return false;
            }
        }

        // B4-iii-a：绑定真源 = 设备级组件行（不再读组件里的每实体副本）
        const hgl::graph::ssbo::MaterialDataRow *material_row = GetMaterialRow();

        if (material_row
         && (material_row->authored_flags & hgl::graph::ssbo::kMaterialDataFlagAuthoredDataResource))
        {
            hgl::graph::GlobalSSBOBinding material_ssbo_binding{
                static_cast<hgl::graph::GlobalSSBOType>(material_row->surface_type),
                material_row->surface_ssbo_id,
                material_row->surface_data_index };
            material_ssbo_binding.ssbo_type =
                hgl::graph::mtl::ResolveRecipeSSBOType(
                    out_recipe,
                    material_ssbo_binding.ssbo_type);

            if (!material_ssbo_binding.IsValid())
                return false;

            out_recipe.material_ssbo_binding = material_ssbo_binding;
        }

        // 数据层边界规范化：写回 mtl_def_id 权威值与解析后的渲染状态，
        // 下游（acquire / 管线创建）不再重复。
        hgl::graph::mtl::NormalizeRecipe(out_recipe);
        return true;
    }

    void MaterialData::OnDetach()
    {
        Component::OnDetach();

        ResetRecipe(recipe_override);
        has_recipe_override = false;
        ClearAuthoredResources();

        // B4-iii-a：**不在析构期归还设备级行**——方案对句柄的定稿是「句柄语义（不是租约）：
        // 长生命周期、**析构不归还行**」；行回收属 arena 自身策略。
        // 实测教训（2026-10-02）：在 OnDetach 里经 GetRegistry() 调 ReleaseID，会在
        // `~GlobalSSBOBufferRegistry → ~ActiveRowPool → ~ActiveIDManager`（GraphicsContext::Shutdown）
        // 期间写坏池状态 ⇒ 堆破坏/双重释放 ⇒ 进程崩在关闭流程，且**丢失未 flush 的日志**
        //（看起来像"初始化就挂"，实为关闭期崩）。
        material_row_id = 0;

        ++authored_generation;
    }
}//namespace hgl::ecs
