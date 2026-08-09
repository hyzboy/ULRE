#include <hgl/shadergen/ActiveProfileBindingViewBuilder.h>

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/MaterialResourceLayout.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl
{
    namespace
    {
        bool SetBuildFailure(
            ActiveProfileBindingViewBuildDiagnostic &diagnostic,
            const ActiveProfileBindingViewBuildError error,
            const TextureSlot texture_slot = TextureSlot::BaseColor,
            const uint32 data_slot = 0,
            const SSBOType ssbo_type = SSBOType::UserDefined) noexcept
        {
            diagnostic.error = error;
            diagnostic.texture_slot = texture_slot;
            diagnostic.data_slot = data_slot;
            diagnostic.ssbo_type = ssbo_type;
            return false;
        }

        uint64 HashText(const std::string &value) noexcept
        {
            if (value.empty())
                return 0;
            return hgl::hash::FNV1aAppendBytes(
                hgl::hash::FNV1aInit<uint64>(),
                value.data(),
                value.size());
        }

        uint64 AppendText(
            uint64 hash,
            const std::string &value) noexcept
        {
            const uint32 length = static_cast<uint32>(value.size());
            hash = hgl::hash::FNV1aAppendValueBytes(hash, length);
            return length > 0
                ? hgl::hash::FNV1aAppendBytes(
                    hash, value.data(), length)
                : hash;
        }

        uint64 HashTextureLogicalResource(
            const SurfaceProfileID profile_id,
            const SurfaceProjectionID projection_id,
            const TextureSlot slot) noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(hash, profile_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, projection_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, DescriptorSemantic::MaterialTexture);
            return hgl::hash::FNV1aAppendValueBytes(hash, slot);
        }

        uint64 HashDataLogicalResource(
            const SurfaceProfileID profile_id,
            const SurfaceProjectionID projection_id,
            const uint32 data_slot,
            const SSBOType ssbo_type) noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(hash, profile_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, projection_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, DescriptorSemantic::MaterialDataSlotData);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, data_slot);
            return hgl::hash::FNV1aAppendValueBytes(hash, ssbo_type);
        }

        uint64 HashTextureMetadata(
            const ActiveProfileTextureBinding &binding) noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.logical_resource_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.profile_slot);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.required);
            return hgl::hash::FNV1aAppendValueBytes(
                hash, binding.allow_fallback);
        }

        uint64 HashDataMetadata(
            const ActiveProfileDataBinding &binding) noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.logical_resource_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.data_slot);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.ssbo_type);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.required);
            return hgl::hash::FNV1aAppendValueBytes(
                hash, binding.allow_fallback);
        }

        uint64 HashNonAssetBinding(
            const uint64 logical_resource_id,
            const ActiveProfileBindingSource source,
            const uint32 value = 0) noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, logical_resource_id);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, source);
            return hgl::hash::FNV1aAppendValueBytes(hash, value);
        }

        ActiveProfileTextureBinding *FindTextureView(
            ActiveProfileBindingView &view,
            const TextureSlot slot) noexcept
        {
            for (int i = 0; i < view.textures.GetCount(); ++i)
            {
                if (view.textures[i].profile_slot == slot)
                    return &view.textures[i];
            }
            return nullptr;
        }

        ActiveProfileDataBinding *FindDataView(
            ActiveProfileBindingView &view,
            const uint32 data_slot,
            const SSBOType ssbo_type) noexcept
        {
            for (int i = 0; i < view.data.GetCount(); ++i)
            {
                if (view.data[i].data_slot == data_slot
                 && view.data[i].ssbo_type == ssbo_type)
                    return &view.data[i];
            }
            return nullptr;
        }

        int FindRecipeTexture(
            const MaterialRecipe &recipe,
            const TextureSlot slot,
            ActiveProfileBindingViewBuildDiagnostic &diagnostic) noexcept
        {
            int found = -1;
            for (int i = 0;
                 i < static_cast<int>(recipe.textures.size());
                 ++i)
            {
                if (recipe.textures[static_cast<size_t>(i)].slot != slot)
                    continue;
                if (found >= 0)
                {
                    SetBuildFailure(
                        diagnostic,
                        ActiveProfileBindingViewBuildError::
                            DuplicateRecipeTexture,
                        slot);
                    return -2;
                }
                found = i;
            }
            return found;
        }

        int FindRecipeData(
            const MaterialRecipe &recipe,
            const uint32 data_slot,
            const SSBOType ssbo_type,
            ActiveProfileBindingViewBuildDiagnostic &diagnostic) noexcept
        {
            int found = -1;
            for (int i = 0;
                 i < static_cast<int>(recipe.ssbo_assets.size());
                 ++i)
            {
                const RecipeSSBOAssetBinding &binding =
                    recipe.ssbo_assets[static_cast<size_t>(i)];
                if (binding.data_slot != data_slot
                 || binding.ssbo_type != ssbo_type)
                    continue;
                if (found >= 0)
                {
                    SetBuildFailure(
                        diagnostic,
                        ActiveProfileBindingViewBuildError::
                            DuplicateRecipeData,
                        TextureSlot::BaseColor,
                        data_slot,
                        ssbo_type);
                    return -2;
                }
                found = i;
            }
            return found;
        }

        template<typename T>
        void SortBindings(ValueArray<T> &bindings)
        {
            for (int i = 1; i < bindings.GetCount(); ++i)
            {
                const T value = bindings[i];
                int insert_at = i;
                while (insert_at > 0
                    && value.logical_resource_id
                        < bindings[insert_at - 1].logical_resource_id)
                {
                    bindings[insert_at] = bindings[insert_at - 1];
                    --insert_at;
                }
                bindings[insert_at] = value;
            }
        }
    }

    const char *GetActiveProfileBindingViewBuildErrorName(
        const ActiveProfileBindingViewBuildError error) noexcept
    {
        switch (error)
        {
        case ActiveProfileBindingViewBuildError::None:
            return "None";
        case ActiveProfileBindingViewBuildError::InvalidPreparedProgramSet:
            return "InvalidPreparedProgramSet";
        case ActiveProfileBindingViewBuildError::DuplicateRecipeTexture:
            return "DuplicateRecipeTexture";
        case ActiveProfileBindingViewBuildError::DuplicateRecipeData:
            return "DuplicateRecipeData";
        case ActiveProfileBindingViewBuildError::InvalidBindingView:
            return "InvalidBindingView";
        }
        return "Unknown";
    }

    const char *GetActiveProfileBindingSourceName(
        const ActiveProfileBindingSource source) noexcept
    {
        switch (source)
        {
        case ActiveProfileBindingSource::Asset:
            return "Asset";
        case ActiveProfileBindingSource::DirectValue:
            return "DirectValue";
        case ActiveProfileBindingSource::Fallback:
            return "Fallback";
        case ActiveProfileBindingSource::Missing:
            return "Missing";
        case ActiveProfileBindingSource::Omitted:
            return "Omitted";
        }
        return "Unknown";
    }

    uint64 GetActiveProfileBindingSourceHash(
        const MaterialRecipe &recipe) noexcept
    {
        uint64 hash = hgl::hash::FNV1aInit<uint64>();
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, MaterialProgramContractSchemaVersion);

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, static_cast<uint32>(recipe.textures.size()));
        for (const RecipeTextureBinding &binding : recipe.textures)
        {
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.slot);
            hash = AppendText(hash, binding.resource_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.direct_value);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.use_direct_value);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.required);
        }

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, static_cast<uint32>(recipe.ssbo_assets.size()));
        for (const RecipeSSBOAssetBinding &binding :
             recipe.ssbo_assets)
        {
            hash = AppendText(hash, binding.data_slot_name);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.data_slot);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.ssbo_type);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.ssbo_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.data_index);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.use_data_index);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.shared_across_instances);
        }
        return hash;
    }

    uint64 GetMaterialTextureAssetIdentityHash(
        const char *resource_id,
        const uint32 resource_id_length) noexcept
    {
        if (!resource_id || resource_id_length == 0)
            return 0;
        return hgl::hash::FNV1aAppendBytes(
            hgl::hash::FNV1aInit<uint64>(),
            resource_id,
            resource_id_length);
    }

    uint64 GetMaterialDataAssetIdentityHash(
        const SSBOType ssbo_type,
        const uint32 ssbo_id,
        const uint32 data_slot) noexcept
    {
        uint64 hash = hgl::hash::FNV1aInit<uint64>();
        hash = hgl::hash::FNV1aAppendValueBytes(hash, ssbo_type);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, ssbo_id);
        return hgl::hash::FNV1aAppendValueBytes(hash, data_slot);
    }

    bool BuildActiveProfileBindingView(
        const MaterialRecipe &recipe,
        const MaterialResourceLayout &resource_layout,
        const PreparedMaterialProgramSet &prepared_set,
        ActiveProfileBindingView &out_view,
        ActiveProfileBindingViewBuildDiagnostic &out_diagnostic) noexcept
    {
        out_view = {};
        out_diagnostic = {};

        const EffectiveMaterialProgramKey *active_program =
            prepared_set.GetExecutableProgram();
        if (!active_program)
            return SetBuildFailure(
                out_diagnostic,
                ActiveProfileBindingViewBuildError::
                    InvalidPreparedProgramSet);

        out_view.effective_material_program_digest =
            active_program->GetDigest();
        out_view.source_binding_hash =
            GetActiveProfileBindingSourceHash(recipe);
        out_view.active_surface_profile_id =
            active_program->resolved_surface_profile_id;
        out_view.active_projection_id = active_program->projection_id;

        bool uses_texture_layer_table = false;
        for (const MaterialResourceRequirement &requirement :
             resource_layout.requirements)
        {
            if (requirement.semantic
                    == DescriptorSemantic::MaterialTextureLayerTable)
            {
                uses_texture_layer_table = true;
                continue;
            }

            const bool material_texture =
                requirement.semantic
                    == DescriptorSemantic::MaterialTexture
             || requirement.semantic
                    == DescriptorSemantic::MaterialSampler;
            if (material_texture)
            {
                ActiveProfileTextureBinding *binding =
                    FindTextureView(out_view, requirement.texture_slot);
                if (!binding)
                {
                    const int binding_index =
                        out_view.textures.Add(
                            ActiveProfileTextureBinding{});
                    binding = &out_view.textures[binding_index];
                    binding->profile_slot = requirement.texture_slot;
                    binding->semantic = requirement.semantic;
                    binding->logical_resource_id =
                        HashTextureLogicalResource(
                            out_view.active_surface_profile_id,
                            out_view.active_projection_id,
                            requirement.texture_slot);
                    binding->allow_fallback = true;
                }
                else if (binding->semantic
                            == DescriptorSemantic::MaterialTexture
                      && requirement.semantic
                            == DescriptorSemantic::MaterialSampler)
                {
                    binding->semantic = requirement.semantic;
                }
                binding->required =
                    binding->required || requirement.required;
                binding->allow_fallback =
                    binding->allow_fallback
                    && requirement.allow_fallback;
                continue;
            }

            if (requirement.semantic
                    == DescriptorSemantic::MaterialDataSlotData
             && requirement.kind == DescriptorKind::SSBO)
            {
                ActiveProfileDataBinding *binding =
                    FindDataView(
                        out_view,
                        requirement.data_slot,
                        requirement.ssbo_type);
                if (!binding)
                {
                    const int binding_index =
                        out_view.data.Add(ActiveProfileDataBinding{});
                    binding = &out_view.data[binding_index];
                    binding->data_slot = requirement.data_slot;
                    binding->ssbo_type = requirement.ssbo_type;
                    binding->logical_resource_id =
                        HashDataLogicalResource(
                            out_view.active_surface_profile_id,
                            out_view.active_projection_id,
                            requirement.data_slot,
                            requirement.ssbo_type);
                    binding->allow_fallback = true;
                }
                binding->required =
                    binding->required || requirement.required;
                binding->allow_fallback =
                    binding->allow_fallback
                    && requirement.allow_fallback;
            }
        }

        if (uses_texture_layer_table)
        {
            for (const RecipeTextureBinding &recipe_binding :
                 recipe.textures)
            {
                if (!recipe_binding.use_direct_value
                 || FindTextureView(
                        out_view, recipe_binding.slot))
                    continue;

                ActiveProfileTextureBinding binding{};
                binding.profile_slot = recipe_binding.slot;
                binding.semantic = DescriptorSemantic::MaterialTexture;
                binding.logical_resource_id =
                    HashTextureLogicalResource(
                        out_view.active_surface_profile_id,
                        out_view.active_projection_id,
                        recipe_binding.slot);
                binding.required = recipe_binding.required;
                binding.allow_fallback = false;
                out_view.textures.Add(binding);
            }
        }

        ValueArray<uint8> used_textures;
        used_textures.Resize(static_cast<int>(recipe.textures.size()));
        ValueArray<uint8> used_data;
        used_data.Resize(static_cast<int>(recipe.ssbo_assets.size()));
        for (int i = 0; i < used_textures.GetCount(); ++i)
            used_textures[i] = 0;
        for (int i = 0; i < used_data.GetCount(); ++i)
            used_data[i] = 0;

        for (int i = 0; i < out_view.textures.GetCount(); ++i)
        {
            ActiveProfileTextureBinding &view_binding =
                out_view.textures[i];
            const int binding_index = FindRecipeTexture(
                recipe, view_binding.profile_slot, out_diagnostic);
            if (binding_index == -2)
                return false;

            if (binding_index >= 0)
            {
                const RecipeTextureBinding &recipe_binding =
                    recipe.textures[static_cast<size_t>(binding_index)];
                used_textures[binding_index] = 1;
                view_binding.required =
                    view_binding.required || recipe_binding.required;
                view_binding.recipe_binding_index =
                    static_cast<uint32>(binding_index);
                if (recipe_binding.use_direct_value)
                {
                    view_binding.source =
                        ActiveProfileBindingSource::DirectValue;
                    view_binding.direct_value =
                        recipe_binding.direct_value;
                    view_binding.asset_identity_hash =
                        HashNonAssetBinding(
                            view_binding.logical_resource_id,
                            view_binding.source,
                            view_binding.direct_value);
                }
                else if (!recipe_binding.resource_id.empty())
                {
                    view_binding.source =
                        ActiveProfileBindingSource::Asset;
                    view_binding.asset_identity_hash =
                        GetMaterialTextureAssetIdentityHash(
                            recipe_binding.resource_id.data(),
                            static_cast<uint32>(
                                recipe_binding.resource_id.size()));
                }
                else
                {
                    view_binding.recipe_binding_index =
                        InvalidMaterialRecipeBindingIndex;
                }
            }

            if (view_binding.recipe_binding_index
                    == InvalidMaterialRecipeBindingIndex)
            {
                view_binding.source = view_binding.required
                    ? ActiveProfileBindingSource::Missing
                    : ActiveProfileBindingSource::Omitted;
                view_binding.asset_identity_hash =
                    HashNonAssetBinding(
                        view_binding.logical_resource_id,
                        view_binding.source);
                if (view_binding.source
                        == ActiveProfileBindingSource::Missing)
                    ++out_view.missing_required_count;
            }
            view_binding.asset_metadata_hash =
                HashTextureMetadata(view_binding);
        }

        for (int i = 0; i < out_view.data.GetCount(); ++i)
        {
            ActiveProfileDataBinding &view_binding = out_view.data[i];
            const int binding_index = FindRecipeData(
                recipe,
                view_binding.data_slot,
                view_binding.ssbo_type,
                out_diagnostic);
            if (binding_index == -2)
                return false;

            if (binding_index >= 0)
            {
                const RecipeSSBOAssetBinding &recipe_binding =
                    recipe.ssbo_assets[static_cast<size_t>(binding_index)];
                used_data[binding_index] = 1;
                view_binding.recipe_binding_index =
                    static_cast<uint32>(binding_index);
                view_binding.source =
                    ActiveProfileBindingSource::Asset;
                view_binding.ssbo_id = recipe_binding.ssbo_id;
                view_binding.data_index = recipe_binding.data_index;
                view_binding.use_data_index =
                    recipe_binding.use_data_index;
                view_binding.shared_across_instances =
                    recipe_binding.shared_across_instances;

                view_binding.asset_identity_hash =
                    GetMaterialDataAssetIdentityHash(
                        recipe_binding.ssbo_type,
                        recipe_binding.ssbo_id,
                        recipe_binding.data_slot);
            }

            if (view_binding.recipe_binding_index
                    == InvalidMaterialRecipeBindingIndex)
            {
                view_binding.source = view_binding.required
                    ? ActiveProfileBindingSource::Missing
                    : ActiveProfileBindingSource::Omitted;
                view_binding.asset_identity_hash =
                    HashNonAssetBinding(
                        view_binding.logical_resource_id,
                        view_binding.source);
                if (view_binding.source
                        == ActiveProfileBindingSource::Missing)
                    ++out_view.missing_required_count;
            }
            view_binding.asset_metadata_hash =
                HashDataMetadata(view_binding);
        }

        for (int i = 0; i < used_textures.GetCount(); ++i)
        {
            if (!used_textures[i])
                ++out_view.unused_recipe_texture_count;
        }
        for (int i = 0; i < used_data.GetCount(); ++i)
        {
            if (!used_data[i])
                ++out_view.unused_recipe_data_count;
        }

        SortBindings(out_view.textures);
        SortBindings(out_view.data);
        if (!out_view.IsValid())
            return SetBuildFailure(
                out_diagnostic,
                ActiveProfileBindingViewBuildError::InvalidBindingView);
        return true;
    }

    bool BuildActiveProfileMaterialRecipe(
        const MaterialRecipe &source_recipe,
        const ActiveProfileBindingView &binding_view,
        MaterialRecipe &out_recipe) noexcept
    {
        out_recipe = {};
        if (!binding_view.IsRuntimeReady()
         || binding_view.source_binding_hash
                != GetActiveProfileBindingSourceHash(source_recipe))
            return false;

        out_recipe = source_recipe;
        out_recipe.textures.clear();
        out_recipe.ssbo_assets.clear();

        for (int i = 0; i < binding_view.textures.GetCount(); ++i)
        {
            const ActiveProfileTextureBinding &view_binding =
                binding_view.textures[i];
            if (view_binding.source
                    == ActiveProfileBindingSource::Fallback
             || view_binding.source
                    == ActiveProfileBindingSource::Omitted)
                continue;
            if (view_binding.source
                    == ActiveProfileBindingSource::Missing
             || view_binding.recipe_binding_index
                    >= source_recipe.textures.size())
                return false;

            const RecipeTextureBinding &recipe_binding =
                source_recipe.textures[
                    view_binding.recipe_binding_index];
            if (recipe_binding.slot != view_binding.profile_slot)
                return false;
            if (view_binding.source
                    == ActiveProfileBindingSource::Asset)
            {
                if (recipe_binding.use_direct_value
                 || GetMaterialTextureAssetIdentityHash(
                        recipe_binding.resource_id.data(),
                        static_cast<uint32>(
                            recipe_binding.resource_id.size()))
                        != view_binding.asset_identity_hash)
                    return false;
            }
            else if (!recipe_binding.use_direct_value
                  || recipe_binding.direct_value
                        != view_binding.direct_value)
                return false;
            out_recipe.textures.push_back(recipe_binding);
        }

        for (int i = 0; i < binding_view.data.GetCount(); ++i)
        {
            const ActiveProfileDataBinding &view_binding =
                binding_view.data[i];
            if (view_binding.source
                    == ActiveProfileBindingSource::Fallback
             || view_binding.source
                    == ActiveProfileBindingSource::Omitted)
                continue;
            if (view_binding.source
                    != ActiveProfileBindingSource::Asset
             || view_binding.recipe_binding_index
                    >= source_recipe.ssbo_assets.size())
                return false;

            const RecipeSSBOAssetBinding &recipe_binding =
                source_recipe.ssbo_assets[
                    view_binding.recipe_binding_index];
            if (recipe_binding.data_slot != view_binding.data_slot
             || recipe_binding.ssbo_type != view_binding.ssbo_type
             || recipe_binding.ssbo_id != view_binding.ssbo_id)
                return false;
            out_recipe.ssbo_assets.push_back(recipe_binding);
        }
        return true;
    }

    bool BuildActiveProfileResourceAcquirePlan(
        const ActiveProfileBindingView &binding_view,
        ResourceAcquirePlan &out_plan) noexcept
    {
        out_plan = {};
        if (!binding_view.IsRuntimeReady())
            return false;

        out_plan.effective_material_program_digest =
            binding_view.effective_material_program_digest;
        for (int i = 0; i < binding_view.textures.GetCount(); ++i)
        {
            const ActiveProfileTextureBinding &binding =
                binding_view.textures[i];
            if (binding.source != ActiveProfileBindingSource::Asset)
                continue;

            ResourceAcquirePlanEntry entry{};
            entry.logical_resource_id = binding.logical_resource_id;
            entry.asset_identity_hash = binding.asset_identity_hash;
            entry.asset_metadata_hash = binding.asset_metadata_hash;
            entry.reason_contract_id =
                binding_view.active_projection_id;
            entry.reason_kind =
                ResourceAcquireReasonKind::SurfaceProjection;
            entry.semantic = binding.semantic;
            entry.texture_slot = binding.profile_slot;
            entry.kind = ResourceAcquireKind::Texture;
            entry.required = binding.required;
            entry.allow_fallback = binding.allow_fallback;
            out_plan.resources.Add(entry);
        }

        for (int i = 0; i < binding_view.data.GetCount(); ++i)
        {
            const ActiveProfileDataBinding &binding =
                binding_view.data[i];
            if (binding.source != ActiveProfileBindingSource::Asset)
                continue;

            ResourceAcquirePlanEntry entry{};
            entry.logical_resource_id = binding.logical_resource_id;
            entry.asset_identity_hash = binding.asset_identity_hash;
            entry.asset_metadata_hash = binding.asset_metadata_hash;
            entry.reason_contract_id =
                binding_view.active_projection_id;
            entry.reason_kind =
                ResourceAcquireReasonKind::SurfaceProjection;
            entry.semantic =
                DescriptorSemantic::MaterialDataSlotData;
            entry.data_slot = binding.data_slot;
            entry.ssbo_type = binding.ssbo_type;
            entry.kind = ResourceAcquireKind::StorageBuffer;
            entry.required = binding.required;
            entry.allow_fallback = binding.allow_fallback;
            out_plan.resources.Add(entry);
        }

        return ValidateResourceAcquirePlan(out_plan);
    }
}
