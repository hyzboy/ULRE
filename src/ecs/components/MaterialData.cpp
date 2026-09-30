#include<hgl/ecs/components/MaterialData.h>
#include<hgl/ecs/support/RenderResource.h>
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

        void ResetDataAuthoringResource(
            MaterialData::MaterialDataAuthoringResource &resource)
        {
            resource.ssbo_type = hgl::graph::GlobalSSBOType::PBRSurface;
            resource.ssbo_id = 0;
            resource.buffer = nullptr;
            resource.element_capacity = 0;
            resource.byte_stride = 0;
            resource.data_index = uint32_t(-1);
            resource.authored = false;
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
        ++authored_generation;
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

        ++authored_generation;
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

    void MaterialData::SetDataResource(
        const MaterialDataAuthoringResource &resource)
    {
        if (resource.ssbo_id == 0)
        {
            if (data_resource.authored)
            {
                ResetDataAuthoringResource(data_resource);
                ++authored_generation;
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

        data_resource = resource;
        data_resource.authored = true;
        ++authored_generation;
    }

    const MaterialData::MaterialDataAuthoringResource *
        MaterialData::GetDataResource() const
    {
        return data_resource.authored
            ? &data_resource : nullptr;
    }

    void MaterialData::ClearAuthoredResources()
    {
        named_texture_resources.Clear();
        ResetDataAuthoringResource(data_resource);
        ++authored_generation;
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

        const MaterialDataAuthoringResource &resource = data_resource;
        if (resource.authored)
        {
            hgl::graph::GlobalSSBOBinding material_ssbo_binding =
                resource.GetGlobalSSBOBinding();
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

        ++authored_generation;
    }
}//namespace hgl::ecs
