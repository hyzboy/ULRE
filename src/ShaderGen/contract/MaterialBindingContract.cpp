#include <hgl/mtl/MaterialBindingContract.h>
#include <hgl/mtl/MaterialRecipe.h>

#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        bool IsValidTextureSource(const BindingSource source) noexcept
        {
            return source >= BindingSource::Asset
                && source <= BindingSource::Omitted;
        }

        bool IsValidDataSource(const BindingSource source) noexcept
        {
            return source == BindingSource::Asset
                || source == BindingSource::Missing
                || source == BindingSource::Omitted;
        }

        bool IsValidTextureBinding(const ResolvedTextureBinding &binding) noexcept
        {
            return binding.logical_resource_id != 0
                && binding.asset_identity_hash != 0
                && binding.asset_metadata_hash != 0
                && (binding.semantic == DescriptorSemantic::MaterialTexture
                 || binding.semantic == DescriptorSemantic::MaterialSampler)
                && IsValidMaterialTextureName(binding.texture_name)
                && binding.texture_layout_index
                    != InvalidMaterialRecipeBindingIndex
                && IsValidTextureSource(binding.source)
                && ((binding.source == BindingSource::Asset)
                    ? binding.recipe_binding_index
                        != InvalidMaterialRecipeBindingIndex
                    : binding.recipe_binding_index
                        == InvalidMaterialRecipeBindingIndex)
                && !(binding.source == BindingSource::Missing
                 && !binding.required)
                && !(binding.source == BindingSource::Omitted
                 && binding.required);
        }

        bool IsValidDataBinding(const ResolvedDataBinding &binding) noexcept
        {
            return binding.logical_resource_id != 0
                && binding.asset_identity_hash != 0
                && binding.asset_metadata_hash != 0
                && binding.semantic == DescriptorSemantic::MaterialPrivateData
                && binding.ssbo_type >= MaterialSSBOType::BEGIN_RANGE
                && binding.ssbo_type <= MaterialSSBOType::END_RANGE
                && IsValidDataSource(binding.source)
                && ((binding.source == BindingSource::Asset)
                    ? binding.recipe_binding_index
                        != InvalidMaterialRecipeBindingIndex
                    : binding.recipe_binding_index
                        == InvalidMaterialRecipeBindingIndex)
                && !(binding.source == BindingSource::Missing
                 && !binding.required)
                && !(binding.source == BindingSource::Omitted
                 && binding.required);
        }

    }

    const char *GetBindingBuildErrorName(
        const BindingBuildError error) noexcept
    {
#define HGL_ERROR(name) case BindingBuildError::name: return #name;
        switch (error)
        {
            HGL_BINDING_BUILD_ERROR_LIST
        }
#undef HGL_ERROR
        return "Unknown";
    }

    const char *GetBindingSourceName(
        const BindingSource source) noexcept
    {
        switch (source)
        {
        case BindingSource::Asset: return "Asset";
        case BindingSource::Missing: return "Missing";
        case BindingSource::Omitted: return "Omitted";
        }
        return "Unknown";
    }

    bool ValidateResolvedBindingTable(
        const ResolvedBindingTable &table) noexcept
    {
        if (table.program_key_digest == 0
         || table.source_binding_hash == 0)
            return false;

        uint32 observed_missing_required = 0;
        for (int i = 0; i < table.textures.GetCount(); ++i)
        {
            const ResolvedTextureBinding &binding = table.textures[i];
            if (!IsValidTextureBinding(binding))
                return false;

            if (binding.source == BindingSource::Missing)
                ++observed_missing_required;

            for (int j = 0; j < i; ++j)
            {
                if (table.textures[j].logical_resource_id == binding.logical_resource_id
                 || std::strcmp(
                        table.textures[j].texture_name,
                        binding.texture_name) == 0)
                    return false;
            }
        }

        for (int i = 0; i < table.data.GetCount(); ++i)
        {
            const ResolvedDataBinding &binding = table.data[i];
            if (!IsValidDataBinding(binding))
                return false;

            if (binding.source == BindingSource::Missing)
                ++observed_missing_required;

            for (int j = 0; j < i; ++j)
            {
                if (table.data[j].logical_resource_id == binding.logical_resource_id
                 || (table.data[j].material_private_data_slot == binding.material_private_data_slot
                  && table.data[j].ssbo_type == binding.ssbo_type))
                    return false;
            }
        }

        return observed_missing_required == table.missing_required_count;
    }

    uint64 GetBindingSourceHash(
        const MaterialRecipe &recipe) noexcept
    {
        hgl::hash::FNV1aHasher64 h;
        h << static_cast<uint32>(recipe.textures.size());
        for (const auto &texture : recipe.textures)
        {
            h << texture.texture_name
              << texture.resource_id;
            h << texture.array_layer
              << texture.required;
        }

        h << static_cast<uint32>(recipe.ssbo_assets.size());
        for (const auto &binding : recipe.ssbo_assets)
        {
            h << binding.ssbo_type
              << binding.ssbo_id
              << binding.data_index
              << binding.use_data_index
              << binding.shared_across_instances;
        }
        return h;
    }

    uint64 GetResolvedTextureAssetIdentityHash(
        const char *resource_id,
        const uint32 resource_id_length) noexcept
    {
        if (!resource_id || resource_id_length == 0)
            return 0;

        hgl::hash::FNV1aHasher64 h;
        h << resource_id_length;
        h.AppendBytes(resource_id, resource_id_length);
        return h;
    }

    uint64 GetResolvedDataAssetIdentityHash(
        const MaterialSSBOType ssbo_type,
        const uint32 ssbo_id,
        const uint32 material_private_data_slot) noexcept
    {
        hgl::hash::FNV1aHasher64 h;
        h << ssbo_type
          << ssbo_id
          << material_private_data_slot;
        return h;
    }

    bool ResolvedBindingTable::IsValid() const noexcept
    {
        return ValidateResolvedBindingTable(*this);
    }

    bool ResolvedBindingTable::IsRuntimeReady() const noexcept
    {
        return IsValid() && missing_required_count == 0;
    }

}
