#include<hgl/ecs/components/MaterialData.h>
#include<hgl/ecs/support/pipeline/RenderResource.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/mtl/MaterialResolver.h>
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
                return resources.Update(key, resource);
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
        // 配方是重类型 ⇒ 落**世界级旁表**（键 = EntityID）。条目建不出来即报错返回。
        MaterialAuthoringSideTable::Entry *entry = GetOrCreateAuthoringEntry();

        if (!entry)
            return;

        entry->recipe_override = recipe;
        entry->has_recipe_override = true;

        TouchAuthored();
    }

    const hgl::graph::mtl::MaterialRecipe *MaterialData::GetRecipeOverride() const
    {
        const MaterialAuthoringSideTable *table = GetAuthoringTable();

        if (!table)
            return nullptr;

        const MaterialAuthoringSideTable::Entry *entry = table->Find(GetOwnerID());

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

        TouchAuthored();
        return true;
    }

    bool MaterialData::SetTextureResource(
        const std::string &name,
        hgl::graph::Texture *texture,
        MaterialTextureResourceKind kind,
        const uint32_t array_layer,
        const bool required)
    {
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
        MaterialTextureResourceKind kind,
        const uint32_t array_layer,
        const bool required)
    {
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

        const MaterialAuthoringSideTable::Entry *side = table->Find(GetOwnerID());

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
        return EvaluateRequiredTextureReadiness()
            == RequiredTextureReadiness::Pending;
    }

    bool
        MaterialData::HasFailedRequiredTexture() const
    {
        return EvaluateRequiredTextureReadiness()
            == RequiredTextureReadiness::Degraded;
    }

    RequiredTextureReadiness
        MaterialData::EvaluateRequiredTextureReadiness() const
    {
        const MaterialAuthoringSideTable *table = GetAuthoringTable();

        if (!table)
            return RequiredTextureReadiness::Ready;

        return hgl::ecs::EvaluateRequiredTextureReadiness(
            table->Find(GetOwnerID()));
    }

    // ── 内容行绑定（2026-11 收敛：真源 = 旁表条目；设备组件行池已退役）───────────

    bool MaterialData::SetDataResource(
        const MaterialDataAuthoringResource &resource)
    {
        if (resource.ssbo_id == 0)
        {
            // 清空路径：有条目就复位引用并推进代数；无条目 ⇒ 本来就没有 ⇒ 直接成功。
            MaterialAuthoringSideTable *table = GetAuthoringTable();
            MaterialAuthoringSideTable::Entry *entry =
                table ? table->Find(GetOwnerID()) : nullptr;

            if (entry && entry->has_data_resource)
            {
                entry->data_resource = {};
                entry->has_data_resource = false;
                TouchAuthored();
            }
            return true;
        }

        if (!resource.GetGlobalSSBOBinding().IsValid())
        {
            GLogError(
                "[MaterialData] Material data resource rejected missing active row ID type=%s ssbo_id=%u data_index=%u",
                hgl::serialize::EnumNameOf(
                    resource.ssbo_type),
                resource.ssbo_id,
                resource.data_index);
            return false;
        }

        MaterialAuthoringSideTable::Entry *entry = GetOrCreateAuthoringEntry();

        if (!entry)
            return false;

        const hgl::graph::GlobalSSBOBinding binding = resource.GetGlobalSSBOBinding();

        entry->data_resource = binding;
        entry->has_data_resource = true;
        TouchAuthored();
        return true;
    }

    // ── 世界级旁表取用（键 = EntityID）──────────────────────────────────────────
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

        return table->GetOrCreate(GetOwnerID());
    }

    const MaterialAuthoringSideTable::Entry *MaterialData::FindAuthoringEntry() const
    {
        const MaterialAuthoringSideTable *table = GetAuthoringTable();

        return table ? table->Find(GetOwnerID()) : nullptr;
    }

    bool MaterialData::HasDataResource() const
    {
        const auto *entry = FindAuthoringEntry();

        return entry && entry->has_data_resource;
    }

    uint32_t MaterialData::GetAuthoredGeneration() const
    {
        const auto *entry = FindAuthoringEntry();

        return entry ? entry->authored_generation : 0u;
    }

    void MaterialData::BumpAuthoredGeneration()
    {
        // 上游来源变化：只动代数，不改授权数据本体。条目不存在 ⇒ 没有任何授权状态
        // 可失效（解析链本来就走资产默认配方/无材质），代数留在 0 即可。
        MaterialAuthoringSideTable *table = GetAuthoringTable();

        if (MaterialAuthoringSideTable::Entry *entry =
                table ? table->Find(GetOwnerID()) : nullptr)
            ++entry->authored_generation;

        // P1：上游来源变化（asset 默认配方 / 变体切换）会让解析结果与 shader 身份失配 ⇒
        // 材质批的键集合可能变化，按拓扑变更记账（而不是动态事实）。
        const Entity *owner = GetOwner();

        if (owner)
            if (ECSContext *context = owner->GetContext())
                context->MarkTopologyChanged(TopologyChange::MaterialBinding);
    }

    // 授权变更唯一入口（见 .h 注释）：代数 +1 + 拓扑记账。
    void MaterialData::TouchAuthored()
    {
        if (MaterialAuthoringSideTable::Entry *entry = GetOrCreateAuthoringEntry())
            ++entry->authored_generation;

        // P1：授权状态变化 = 材质批的划分依据可能变（从 Basic 升到 Textured
        // 会换 shader 身份 ⇒ 换 MaterialBatch 键）。归到拓扑类别，让快照拒绝复用。
        if (const Entity *owner = GetOwner())
            if (ECSContext *context = owner->GetContext())
                context->MarkTopologyChanged(TopologyChange::MaterialBinding);
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

        hgl::graph::mtl::ResolvedMaterial definition{};
        const bool has_definition =
            hgl::graph::mtl::ResolveMaterialFromRecipe(out_recipe, definition);

        // 具名纹理资源住在**世界级旁表**（键 = EntityID）——解析路径读旁表。
        // 无旁表/无条目 ⇒ 空表（语义 = 没有任何具名纹理授权）。
        static const hgl::UnorderedMap<hgl::AnsiString, MaterialTextureAuthoringResource>
            kEmptyNamedTextures;

        const MaterialAuthoringSideTable::Entry *authoring_entry = FindAuthoringEntry();

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

        // 绑定真源 = 旁表条目（2026-11：设备级组件行退役后唯一的一处）
        if (authoring_entry && authoring_entry->has_data_resource)
        {
            hgl::graph::GlobalSSBOBinding material_ssbo_binding =
                authoring_entry->data_resource;
            material_ssbo_binding.ssbo_type =
                hgl::graph::mtl::ResolveRecipeSSBOType(
                    out_recipe,
                    material_ssbo_binding.ssbo_type);

            if (!material_ssbo_binding.IsValid())
                return false;

            out_recipe.material_ssbo_binding = material_ssbo_binding;
        }

        // 数据层边界规范化：写回领域契约默认值与解析后的渲染状态，
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
                          out_recipe.recipe_name.c_str(),
                          hgl::graph::mtl::GetMaterialDomainName(rule.domain),
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
        // 重类型授权数据（配方覆盖 / 具名纹理 / 内容行绑定 / 代数）随组件分离而**摘除**
        //（键 = EntityID；条目整个删除 ⇒ 分离后所有读数回到「无授权」初态）。
        // 注意：**不在析构期碰任何设备级池** —— 实测教训（2026-10-02）：OnDetach 期间
        // 写池状态会在 GraphicsContext::Shutdown 序列里造成堆破坏/双重释放。
        // 旁表是世界级对象、随 Context 生命周期存在，Remove 只是哈希表删项，安全。
        if (MaterialAuthoringSideTable *table = GetAuthoringTable())
            table->Remove(GetOwnerID());
    }
}//namespace hgl::ecs
