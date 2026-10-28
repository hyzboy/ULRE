#include<hgl/ecs/components/MaterialData.h>
#include<hgl/ecs/support/RenderResource.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    namespace
    {
        bool UpsertTextureAuthoringResource(
            hgl::UnorderedMap<hgl::AnsiString,
                MaterialTextureAuthoringResource> &resources,
            const std::string &name,
            const MaterialTextureAuthoringResource &resource)
        {
            const hgl::AnsiString key(name.c_str());
            if (resources.GetValuePointer(key))
                return resources.Change(key, resource);
            return resources.Add(key, resource);
        }

        bool AppendTextureBinding(
            hgl::graph::mtl::MaterialRecipe &recipe,
            const std::string &name,
            const MaterialTextureAuthoringResource &resource)
        {
            // 统一句柄语义：只要有来源就产出绑定 —— 只有文件名的句柄同样产出绑定，
            // 身份用 `texfile:<path>`（就绪前后不变），图元就绪后即可直接命中 bindless 资源。
            // 采样器不参与绑定（Bindless 预设集中管理），故这里没有任何 sampler 判据。
            if (!resource.HasSource())
                return true;

            const std::string resource_id = BuildTextureResourceId(resource.asset);
            if (resource_id.empty())
                return false;

            return hgl::graph::mtl::UpsertRecipeTextureBinding(
                recipe,
                name,
                resource_id,
                resource.required,
                resource.array_layer,
                resource.asset);
        }
    }

    void MaterialData::SetRecipe(const hgl::graph::mtl::MaterialRecipe &recipe)
    {
        // B4-iii-b：配方是重类型 ⇒ 落**世界级旁表**（键 = 本组件的材质行行号）；
        // 行内只留授权位 + revision。行不可用即报错返回，不造半个对象。
        MaterialAuthoringSideTable::Entry *entry = GetOrCreateAuthoringEntry();

        if (!entry)
            return;

        entry->recipe_override = recipe;
        entry->has_recipe_override = true;

        TouchAuthoredRow(hgl::graph::ssbo::kMaterialDataFlagAuthoredRecipe, 0);
    }

    const hgl::graph::mtl::MaterialRecipe *MaterialData::GetRecipeOverride() const
    {
        const MaterialAuthoringSideTable *table = GetAuthoringTable();

        if (!table)
            return nullptr;

        const MaterialAuthoringSideTable::Entry *entry = table->Find(material_row_id);

        return (entry && entry->has_recipe_override) ? &entry->recipe_override : nullptr;
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
        // 统一句柄：只拒绝「根本没表达纹理」（无句柄 / 句柄既无纹理也无文件名）。
        // **只有文件名的句柄是合法状态** —— 由 ECS 侧驱动异步加载。
        // 采样器不是授权数据（Bindless 预设统一管理）⇒ 这里不再校验。
        if (!resource.HasSource())
        {
            GLogError(
                "[MaterialData] Texture authoring rejected empty handle name=%s",
                name.c_str());
            return false;
        }

        MaterialAuthoringSideTable::Entry *entry = GetOrCreateAuthoringEntry();

        if (!entry)
            return false;

        if (!UpsertTextureAuthoringResource(
                entry->named_texture_resources,
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
        const uint32_t array_layer,
        const bool required)
    {
        // `sampler` 已废弃：Bindless 预设集中管理采样器，材质侧不再持有它。
        (void)sampler;

        if (!texture)
        {
            GLogError(
                "[MaterialData] Texture authoring rejected null texture name=%s",
                name.c_str());
            return false;
        }

        // 便捷重载不新增行为路径：把现成纹理包成句柄（Resident）后走统一入口。
        hgl::graph::TextureManager *tex_manager = nullptr;
        if (const Entity *owner = GetOwner())
            if (ECSContext *world = owner->GetContext())
                if (hgl::graph::GraphicsContext *gc = world->GetGraphicsContext())
                    tex_manager = gc->GetTextureManager();

        if (!tex_manager)
        {
            GLogError(
                "[MaterialData] Texture authoring rejected no TextureManager name=%s",
                name.c_str());
            return false;
        }

        hgl::graph::TextureAsset *asset = tex_manager->WrapTextureAsset(texture);

        if (!asset)
        {
            GLogError(
                "[MaterialData] Texture authoring rejected wrap failed name=%s",
                name.c_str());
            return false;
        }

        MaterialTextureAuthoringResource resource{};
        resource.asset        = asset;
        resource.kind         = kind;
        resource.array_layer  = array_layer;
        resource.required     = required;

        return SetTextureResource(name, resource);
    }

    bool MaterialData::SetTextureResource(
        const std::string &name,
        hgl::graph::TextureAsset *asset,
        hgl::graph::Sampler *sampler,
        MaterialTextureResourceKind kind,
        const uint32_t array_layer,
        const bool required)
    {
        // 与 Texture* 重载同理：`sampler` 已废弃（Bindless 预设集中管理），只是被忽略。
        (void)sampler;

        if (!asset)
        {
            GLogError(
                "[MaterialData] Texture authoring rejected null asset name=%s",
                name.c_str());
            return false;
        }

        // 与 Texture* 重载同理：不新增行为路径，只是把句柄直接落库。
        // 「只有文件名」的句柄在这里是**合法**的 —— 加载由 ECS 侧驱动。
        MaterialTextureAuthoringResource resource{};
        resource.asset        = asset;
        resource.kind         = kind;
        resource.array_layer  = array_layer;
        resource.required     = required;

        return SetTextureResource(name, resource);
    }

    const MaterialTextureAuthoringResource *
        MaterialData::GetTextureResource(
            const std::string &name) const
    {
        if (!hgl::graph::mtl::IsValidMaterialTextureName(name))
            return nullptr;

        const MaterialAuthoringSideTable *table = GetAuthoringTable();

        if (!table)
            return nullptr;

        const MaterialAuthoringSideTable::Entry *side = table->Find(material_row_id);

        if (!side)
            return nullptr;

        const hgl::AnsiString key(name.c_str());
        if (const auto *entry = side->named_texture_resources.GetValuePointer(key))
        {
            // 只要求「表达了某个纹理」：只有文件名的句柄也要能被看到（渲染层据 IsReady()
            // 判断可否绑定，未就绪的图元由 CanRender 拦截，不会走到绑定路径）。
            if (entry->HasSource())
                return entry;
        }
        return nullptr;
    }

    bool
        MaterialData::HasPendingRequiredTexture() const
    {
        const MaterialAuthoringSideTable *table = GetAuthoringTable();

        if (!table)
            return false;

        const MaterialAuthoringSideTable::Entry *side = table->Find(material_row_id);

        if (!side)
            return false;

        for (const auto &[named_key, resource] : side->named_texture_resources)
        {
            // 只对「必需 + 确实声明了来源」的条目把关：既无纹理又无文件名的空条目
            // 本来就不会被收集路径当作有效绑定，不该把图元永久钉在不可渲染状态。
            if (resource.required && resource.HasSource() && !resource.IsReady())
                return true;
        }

        return false;
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

    // ── B4-iii-b：世界级旁表取用（键 = 材质行行号）──────────────────────────
    MaterialAuthoringSideTable *MaterialData::GetAuthoringTable() const
    {
        const Entity *owner = GetOwner();
        ECSContext *world = owner ? owner->GetContext() : nullptr;

        return world ? world->GetMaterialAuthoringTable() : nullptr;
    }

    MaterialAuthoringSideTable::Entry *MaterialData::GetOrCreateAuthoringEntry()
    {
        MaterialAuthoringSideTable *table = GetAuthoringTable();

        if (!table)
        {
            GLogError("[MaterialData] 无世界/无授权旁表：重类型授权数据无处安放 owner_id=%llu",
                      GetOwnerID().index);
            return nullptr;
        }

        if (!EnsureMaterialRow())      // 行 = 旁表的键；行不可用即失败（已报错）
            return nullptr;

        return table->GetOrCreate(material_row_id);
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

        // B4-iii-b：具名纹理资源住在**世界级旁表**（键 = 材质行行号）——解析路径读旁表，
        // 组件已无副本。无旁表/无条目 ⇒ 空表（语义 = 没有任何具名纹理授权）。
        static const hgl::UnorderedMap<hgl::AnsiString, MaterialTextureAuthoringResource>
            kEmptyNamedTextures;

        const MaterialAuthoringSideTable *authoring_table = GetAuthoringTable();
        const MaterialAuthoringSideTable::Entry *authoring_entry =
            authoring_table ? authoring_table->Find(material_row_id) : nullptr;

        const auto &named_texture_resources = authoring_entry
            ? authoring_entry->named_texture_resources
            : kEmptyNamedTextures;

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

        // A7c：资源级材质错误 —— 定义声明了 `required` 纹理，但配方里没有实际纹理资源。
        // NormalizeRecipe 只认**定义 ID 级**错误（无来源 / ID 不可解析）；纹理是否真的给了
        // 资源要到本层才可见（具名纹理住在世界级旁表），所以判据挂在这里。
        //
        // 语义选择：这类错误**回退成错误棋盘格**（标记色青）而不是剔除图元 —— 作者写了
        // `required = true` 表达「该材质必须带这张纹理」，缺内容必须被看见，而不是让图元
        // 静默消失。（纹理名非法/重复/未声明仍是硬错误，见图元校验。）
        if (out_recipe.fallback_error_kind == hgl::graph::mtl::MaterialErrorKind::None)
        {
            const hgl::graph::mtl::MaterialErrorKind resource_error =
                hgl::graph::mtl::ClassifyMaterialResourceError(definition, out_recipe);

            if (resource_error != hgl::graph::mtl::MaterialErrorKind::None)
            {
                const hgl::graph::mtl::FallbackMaterialRule &rule =
                    hgl::graph::mtl::GetFallbackMaterialRule(resource_error);

                GLogError(u8"[MaterialFallback] 材质 \"%s\" 声明为 required 的纹理缺少实际资源 "
                          u8"=> 回退错误材质 %s（棋盘格；错误种类 %s，颜色=(%.2f,%.2f,%.2f,%.2f)）",
                          out_recipe.mtl_def_id.c_str(), rule.definition_id,
                          rule.marker_name,
                          rule.marker_color[0], rule.marker_color[1],
                          rule.marker_color[2], rule.marker_color[3]);

                hgl::graph::mtl::BuildFallbackMaterialRecipe(out_recipe, resource_error);
            }
        }

        return true;
    }

    void MaterialData::OnDetach()
    {
        // B4-iii-b：重类型授权数据（配方覆盖 / 具名纹理资源）随组件分离而**摘除**
        //（键 = 行号；行本身不归还——句柄语义，行回收属 arena 自身策略）。
        if (material_row_id != 0)
        {
            if (MaterialAuthoringSideTable *table = GetAuthoringTable())
                table->Remove(material_row_id);
        }

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
