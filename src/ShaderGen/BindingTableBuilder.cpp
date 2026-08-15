#include <hgl/mtl/BindingTableBuilder.h>

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/ShaderResourceSchema.h>
#include <hgl/shadergen/CanonicalShaderContract.h>
#include <hgl/util/hash/FNV1a.h>

#include "common/CanonicalContractWriter.h"

namespace hgl::graph::mtl
{
    using namespace hgl::graph::shadergen;
    namespace
    {
        bool SetBuildFailure(
            BindingBuildDiagnostic &diagnostic,
            const BindingBuildError error,
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

        uint64 HashNonAssetBinding(
            const uint64 logical_resource_id,
            const BindingSource source,
            const uint32 value = 0) noexcept
        {
            hgl::hash::FNV1aHasher64 h;
            h << logical_resource_id
              << source
              << value;
            return h;
        }

        uint64 ResolveFallbackResourceID(
            const uint64 program_key_digest,
            const DescriptorSemantic semantic,
            const TextureSlot texture_slot,
            const uint32 data_slot,
            const SSBOType ssbo_type,
            const uint64 resource_schema_id) noexcept
        {
            hgl::hash::FNV1aHasher64 h;
            h << program_key_digest
              << resource_schema_id
              << semantic
              << texture_slot
              << data_slot
              << ssbo_type;
            return h != 0 ? h.Result() : 1;
        }

        uint64 HashTextureMetadata(
            const ResolvedTextureBinding &binding) noexcept
        {
            hgl::hash::FNV1aHasher64 h;
            h << binding.logical_resource_id
              << binding.semantic
              << binding.texture_slot
              << binding.recipe_binding_index
              << binding.direct_value
              << binding.source
              << binding.required
              << binding.allow_fallback;
            return h;
        }

        uint64 HashDataMetadata(
            const ResolvedDataBinding &binding) noexcept
        {
            hgl::hash::FNV1aHasher64 h;
            h << binding.logical_resource_id
              << binding.semantic
              << binding.data_slot
              << binding.ssbo_id
              << binding.data_index
              << binding.ssbo_type
              << binding.source
              << binding.use_data_index
              << binding.shared_across_instances
              << binding.required
              << binding.allow_fallback;
            return h;
        }

        uint64 ResolveDescriptorLogicalResourceID(
            const ShaderDescriptorContractEntry &entry,
            const uint64 program_key_digest) noexcept
        {
            if (entry.logical_resource_id != 0)
                return entry.logical_resource_id;

            hgl::hash::FNV1aHasher64 h;
            h << program_key_digest
              << entry.resource_schema_id
              << entry.semantic
              << entry.texture_slot
              << entry.data_slot
              << entry.ssbo_type
              << entry.kind;
            return h != 0 ? h.Result() : 1;
        }

        uint64 MakeViewFallbackTextureID(
            const uint64 program_key_digest,
            const TextureSlot slot) noexcept
        {
            return ResolveFallbackResourceID(
                program_key_digest,
                DescriptorSemantic::MaterialTexture,
                slot,
                0,
                SSBOType::UserDefined,
                0);
        }

        ResolvedTextureBinding *FindTextureBinding(
            ResolvedBindingTable &table,
            const TextureSlot slot) noexcept
        {
            for (int i = 0; i < table.textures.GetCount(); ++i)
            {
                if (table.textures[i].texture_slot == slot)
                    return &table.textures[i];
            }
            return nullptr;
        }

        ResolvedDataBinding *FindDataBinding(
            ResolvedBindingTable &table,
            const uint32 data_slot,
            const SSBOType ssbo_type) noexcept
        {
            for (int i = 0; i < table.data.GetCount(); ++i)
            {
                if (table.data[i].data_slot == data_slot
                 && table.data[i].ssbo_type == ssbo_type)
                    return &table.data[i];
            }
            return nullptr;
        }

        int FindRecipeTexture(
            const MaterialRecipe &recipe,
            const TextureSlot slot,
            BindingBuildDiagnostic &diagnostic) noexcept
        {
            int found = -1;
            for (int i = 0;
                 i < static_cast<int>(recipe.textures.size());
                 ++i)
            {
                if (recipe.textures[static_cast<size_t>(i)].slot_name != GetTextureSlotName(slot))
                    continue;
                if (found >= 0)
                {
                    SetBuildFailure(
                        diagnostic,
                        BindingBuildError::DuplicateRecipeTexture,
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
            BindingBuildDiagnostic &diagnostic) noexcept
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
                        BindingBuildError::DuplicateRecipeData,
                        TextureSlot::BaseColor,
                        data_slot,
                        ssbo_type);
                    return -2;
                }
                found = i;
            }
            return found;
        }

        void SortBindings(ValueArray<ResolvedTextureBinding> &bindings)
        {
            contract_detail::CanonicalSort(bindings, [](const ResolvedTextureBinding &lhs,
                                                        const ResolvedTextureBinding &rhs)
            {
                return lhs.logical_resource_id < rhs.logical_resource_id;
            });
        }

        void SortBindings(ValueArray<ResolvedDataBinding> &bindings)
        {
            contract_detail::CanonicalSort(bindings, [](const ResolvedDataBinding &lhs,
                                                        const ResolvedDataBinding &rhs)
            {
                if (lhs.logical_resource_id != rhs.logical_resource_id)
                    return lhs.logical_resource_id < rhs.logical_resource_id;
                if (lhs.data_slot != rhs.data_slot)
                    return lhs.data_slot < rhs.data_slot;
                return lhs.ssbo_type < rhs.ssbo_type;
            });
        }

        bool AppendLayoutRequirements(
            const ShaderResourceSchema &layout,
            ValueArray<ShaderDescriptorContractEntry> &out_requirements) noexcept
        {
            out_requirements.Clear();
            out_requirements.Reserve(static_cast<int>(layout.resources.size()));

            for (const ShaderResourceSlot &req : layout.resources)
            {
                ShaderDescriptorContractEntry entry{};
                entry.logical_resource_id = req.logical_resource_id;
                entry.resource_schema_id = req.resource_schema_id;
                entry.semantic = req.semantic;
                entry.semantic_layer = req.semantic_layer;
                entry.set_type = req.set_type;
                entry.kind = req.kind;
                entry.texture_slot = req.texture_slot;
                entry.ssbo_type = req.ssbo_type;
                entry.data_slot = req.data_slot;
                entry.stage_flags = req.stage_flags;
                entry.array_count = 1;
                entry.required = req.required;
                entry.allow_fallback = req.allow_fallback;
                out_requirements.Add(entry);
            }

            return true;
        }

        bool BuildBindingTableFromRequirements(
            const MaterialRecipe &recipe,
            const ValueArray<ShaderDescriptorContractEntry> &requirements,
            const uint64 program_key_digest,
            ResolvedBindingTable &out_table,
            BindingBuildDiagnostic &out_diagnostic) noexcept
        {
            out_table = {};
            out_diagnostic = {};
            if (program_key_digest == 0)
                return SetBuildFailure(
                    out_diagnostic,
                    BindingBuildError::InvalidShaderProgramKey);

            out_table.program_key_digest = program_key_digest;
            out_table.source_binding_hash = GetBindingSourceHash(recipe);

            bool uses_texture_layer_table = false;
            for (const ShaderDescriptorContractEntry &entry : requirements)
            {
                if (entry.semantic == DescriptorSemantic::MaterialTextureLayerTable)
                {
                    uses_texture_layer_table = true;
                    continue;
                }

                if (entry.semantic == DescriptorSemantic::MaterialTexture
                 || entry.semantic == DescriptorSemantic::MaterialSampler)
                {
                    const uint64 logical_resource_id =
                        ResolveDescriptorLogicalResourceID(entry, program_key_digest);
                    ResolvedTextureBinding *binding =
                        FindTextureBinding(out_table, entry.texture_slot);
                    if (!binding)
                    {
                        const int index = out_table.textures.Add(
                            ResolvedTextureBinding{});
                        binding = &out_table.textures[index];
                        binding->logical_resource_id = logical_resource_id;
                        binding->semantic = entry.semantic;
                        binding->texture_slot = entry.texture_slot;
                        binding->required = entry.required;
                        binding->allow_fallback = entry.allow_fallback;
                    }
                    else
                    {
                        if (binding->logical_resource_id != logical_resource_id
                         && binding->logical_resource_id != 0
                         && logical_resource_id != 0)
                            return SetBuildFailure(
                                out_diagnostic,
                                BindingBuildError::InvalidBindingTable);
                        if (binding->semantic == DescriptorSemantic::MaterialTexture
                         && entry.semantic == DescriptorSemantic::MaterialSampler)
                            binding->semantic = DescriptorSemantic::MaterialSampler;
                        binding->required = binding->required || entry.required;
                        binding->allow_fallback = binding->allow_fallback && entry.allow_fallback;
                    }
                    continue;
                }

                if (entry.semantic == DescriptorSemantic::MaterialDataSlotData)
                {
                    const uint64 logical_resource_id =
                        ResolveDescriptorLogicalResourceID(entry, program_key_digest);
                    ResolvedDataBinding *binding =
                        FindDataBinding(out_table, entry.data_slot, entry.ssbo_type);
                    if (!binding)
                    {
                        const int index = out_table.data.Add(ResolvedDataBinding{});
                        binding = &out_table.data[index];
                        binding->logical_resource_id = logical_resource_id;
                        binding->semantic = entry.semantic;
                        binding->data_slot = entry.data_slot;
                        binding->ssbo_type = entry.ssbo_type;
                        binding->required = entry.required;
                        binding->allow_fallback = entry.allow_fallback;
                    }
                    else
                    {
                        if (binding->logical_resource_id != logical_resource_id
                         && binding->logical_resource_id != 0
                         && logical_resource_id != 0)
                            return SetBuildFailure(
                                out_diagnostic,
                                BindingBuildError::InvalidBindingTable);
                        binding->required = binding->required || entry.required;
                        binding->allow_fallback = binding->allow_fallback && entry.allow_fallback;
                    }
                }
            }

            if (uses_texture_layer_table)
            {
                for (const RecipeTextureBinding &recipe_binding : recipe.textures)
                {
                    // Bindless materials sample textures through the
                    // MaterialTextureLayerTable SSBO, so every recipe texture
                    // binding (direct-value or asset-source) must appear in the
                    // binding table. Asset bindings feed registration in
                    // PrepareActivePlanResources and round-trip back through
                    // CopyBackMaterialRecipe.
                    TextureSlot slot;
                    if (!ParseTextureSlotName(recipe_binding.slot_name, slot))
                        continue;

                    if (FindTextureBinding(out_table, slot))
                        continue;

                    ResolvedTextureBinding binding{};
                    binding.logical_resource_id = MakeViewFallbackTextureID(
                        program_key_digest, slot);
                    binding.semantic = DescriptorSemantic::MaterialTexture;
                    binding.texture_slot = slot;
                    binding.required = recipe_binding.required;
                    binding.allow_fallback = false;
                    out_table.textures.Add(binding);
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

            for (int i = 0; i < out_table.textures.GetCount(); ++i)
            {
                ResolvedTextureBinding &binding = out_table.textures[i];
                const int binding_index = FindRecipeTexture(
                    recipe, binding.texture_slot, out_diagnostic);
                if (binding_index == -2)
                    return false;

                if (binding_index >= 0)
                {
                    const RecipeTextureBinding &recipe_binding =
                        recipe.textures[static_cast<size_t>(binding_index)];
                    used_textures[binding_index] = 1;
                    binding.recipe_binding_index =
                        static_cast<uint32>(binding_index);
                    binding.required =
                        binding.required || recipe_binding.required;
                    if (recipe_binding.use_direct_value)
                    {
                        binding.source = BindingSource::DirectValue;
                        binding.direct_value = recipe_binding.direct_value;
                        binding.asset_identity_hash = HashNonAssetBinding(
                            binding.logical_resource_id,
                            binding.source,
                            binding.direct_value);
                    }
                    else if (!recipe_binding.resource_id.empty())
                    {
                        binding.source = BindingSource::Asset;
                        binding.asset_identity_hash =
                            GetResolvedTextureAssetIdentityHash(
                                recipe_binding.resource_id.data(),
                                static_cast<uint32>(recipe_binding.resource_id.size()));
                    }
                    else
                    {
                        binding.recipe_binding_index =
                            InvalidMaterialRecipeBindingIndex;
                    }
                }

                if (binding.recipe_binding_index
                        == InvalidMaterialRecipeBindingIndex)
                {
                    binding.source = binding.required
                        ? BindingSource::Missing
                        : BindingSource::Omitted;
                    binding.asset_identity_hash = HashNonAssetBinding(
                        binding.logical_resource_id,
                        binding.source);
                    if (binding.source == BindingSource::Missing)
                        ++out_table.missing_required_count;
                }

                binding.asset_metadata_hash = HashTextureMetadata(binding);
            }

            for (int i = 0; i < out_table.data.GetCount(); ++i)
            {
                ResolvedDataBinding &binding = out_table.data[i];
                const int binding_index = FindRecipeData(
                    recipe,
                    binding.data_slot,
                    binding.ssbo_type,
                    out_diagnostic);
                if (binding_index == -2)
                    return false;

                if (binding_index >= 0)
                {
                    const RecipeSSBOAssetBinding &recipe_binding =
                        recipe.ssbo_assets[static_cast<size_t>(binding_index)];
                    used_data[binding_index] = 1;
                    binding.recipe_binding_index =
                        static_cast<uint32>(binding_index);
                    binding.source = BindingSource::Asset;
                    binding.ssbo_id = recipe_binding.ssbo_id;
                    binding.data_index = recipe_binding.data_index;
                    binding.use_data_index = recipe_binding.use_data_index;
                    binding.shared_across_instances =
                        recipe_binding.shared_across_instances;
                    binding.asset_identity_hash = GetResolvedDataAssetIdentityHash(
                        recipe_binding.ssbo_type,
                        recipe_binding.ssbo_id,
                        recipe_binding.data_slot);
                }

                if (binding.recipe_binding_index
                        == InvalidMaterialRecipeBindingIndex)
                {
                    binding.source = binding.required
                        ? BindingSource::Missing
                        : BindingSource::Omitted;
                    binding.asset_identity_hash = HashNonAssetBinding(
                        binding.logical_resource_id,
                        binding.source);
                    if (binding.source == BindingSource::Missing)
                        ++out_table.missing_required_count;
                }

                binding.asset_metadata_hash = HashDataMetadata(binding);
            }

            for (int i = 0; i < used_textures.GetCount(); ++i)
            {
                if (!used_textures[i])
                    ++out_table.unused_recipe_texture_count;
            }
            for (int i = 0; i < used_data.GetCount(); ++i)
            {
                if (!used_data[i])
                    ++out_table.unused_recipe_data_count;
            }

            SortBindings(out_table.textures);
            SortBindings(out_table.data);

            if (!out_table.IsValid())
                return SetBuildFailure(
                    out_diagnostic,
                    BindingBuildError::InvalidBindingTable);
            return true;
        }

        bool CopyBackMaterialRecipe(
            const ResolvedBindingTable &binding_table,
            const MaterialRecipe &source_recipe,
            MaterialRecipe &out_recipe) noexcept
        {
            out_recipe = source_recipe;
            out_recipe.textures.clear();
            out_recipe.ssbo_assets.clear();

            for (int i = 0; i < binding_table.textures.GetCount(); ++i)
            {
                const ResolvedTextureBinding &binding = binding_table.textures[i];
                if (binding.source == BindingSource::Omitted)
                    continue;
                if (binding.source == BindingSource::Missing
                 || binding.recipe_binding_index
                        >= source_recipe.textures.size())
                    return false;

                const RecipeTextureBinding &recipe_binding =
                    source_recipe.textures[binding.recipe_binding_index];
                if (recipe_binding.slot_name != GetTextureSlotName(binding.texture_slot))
                    return false;
                if (binding.source == BindingSource::Asset)
                {
                    if (recipe_binding.use_direct_value
                     || GetResolvedTextureAssetIdentityHash(
                            recipe_binding.resource_id.data(),
                            static_cast<uint32>(recipe_binding.resource_id.size()))
                            != binding.asset_identity_hash)
                        return false;
                }
                else if (!recipe_binding.use_direct_value
                      || recipe_binding.direct_value != binding.direct_value)
                {
                    return false;
                }
                out_recipe.textures.push_back(recipe_binding);
            }

            for (int i = 0; i < binding_table.data.GetCount(); ++i)
            {
                const ResolvedDataBinding &binding = binding_table.data[i];
                if (binding.source == BindingSource::Omitted)
                    continue;
                if (binding.source != BindingSource::Asset
                 || binding.recipe_binding_index
                        >= source_recipe.ssbo_assets.size())
                    return false;

                const RecipeSSBOAssetBinding &recipe_binding =
                    source_recipe.ssbo_assets[binding.recipe_binding_index];
                if (recipe_binding.data_slot != binding.data_slot
                 || recipe_binding.ssbo_type != binding.ssbo_type
                 || recipe_binding.ssbo_id != binding.ssbo_id)
                    return false;
                out_recipe.ssbo_assets.push_back(recipe_binding);
            }

            return true;
        }
    }

    bool BuildBindingTable(
        const MaterialRecipe &recipe,
        const ShaderResourceSchema &layout,
        const shadergen::ShaderProgramKey &program_key,
        ResolvedBindingTable &out_table,
        BindingBuildDiagnostic &out_diagnostic) noexcept
    {
        ValueArray<ShaderDescriptorContractEntry> requirements;
        if (!AppendLayoutRequirements(layout, requirements))
            return SetBuildFailure(
                out_diagnostic,
                BindingBuildError::InvalidBindingTable);
        return BuildBindingTableFromRequirements(
            recipe,
            requirements,
            program_key.GetDigest(),
            out_table,
            out_diagnostic);
    }

    bool BuildBindingTableRecipe(
        const MaterialRecipe &source_recipe,
        const ResolvedBindingTable &binding_table,
        MaterialRecipe &out_recipe) noexcept
    {
        if (!binding_table.IsRuntimeReady()
         || binding_table.source_binding_hash
                != GetBindingSourceHash(source_recipe))
        {
            out_recipe = {};
            return false;
        }
        return CopyBackMaterialRecipe(binding_table, source_recipe, out_recipe);
    }
}
