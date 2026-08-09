#include <hgl/shadergen/ShadowResourceAcquirePlanBuilder.h>

#include <hgl/util/hash/FNV1a.h>
#include <vector>

namespace hgl::graph::mtl
{
    namespace
    {
        bool SetPlanFailure(
            ShadowResourcePlanDiagnostic &diagnostic,
            const ShadowResourcePlanError error,
            const uint64 logical_resource_id = 0,
            const TextureSlot texture_slot = TextureSlot::BaseColor,
            const uint32 data_slot = 0,
            const SSBOType ssbo_type = SSBOType::UserDefined) noexcept
        {
            diagnostic.error = error;
            diagnostic.logical_resource_id = logical_resource_id;
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

        uint64 HashTextureMetadata(
            const ShaderDescriptorContractEntry &contract,
            const RecipeTextureBinding &binding) noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, contract.resource_schema_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.slot);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.required);
            return hash;
        }

        uint64 HashSSBOAssetIdentity(
            const SSBOType ssbo_type,
            const uint32 ssbo_id,
            const uint32 data_slot) noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, ssbo_type);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, ssbo_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, data_slot);
            return hash;
        }

        uint64 HashSSBOAssetIdentity(
            const RecipeSSBOAssetBinding &binding) noexcept
        {
            return HashSSBOAssetIdentity(
                binding.ssbo_type,
                binding.ssbo_id,
                binding.data_slot);
        }

        uint64 HashSSBOMetadata(
            const ShaderDescriptorContractEntry &contract,
            const RecipeSSBOAssetBinding &binding) noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, contract.resource_schema_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.ssbo_type);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.data_slot);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, binding.shared_across_instances);
            return hash;
        }

        bool GraphContainsModule(
            const ResolvedModuleGraph &graph,
            const ShaderContractStableID module_id) noexcept
        {
            for (int i = 0; i < graph.modules.GetCount(); ++i)
            {
                if (graph.modules[i].module_id == module_id)
                    return true;
            }
            return false;
        }

        ShaderContractStableID FindTextureReasonModule(
            const GLSLCodeModuleRegistry &registry,
            const ResolvedModuleGraph &graph,
            const ShaderDescriptorContractEntry &contract)
        {
            for (int i = 0; i < registry.GetCount(); ++i)
            {
                const GLSLCodeModuleDefinition *definition =
                    registry.GetModuleByIndex(i);
                if (!definition)
                    continue;

                const ShaderContractStableID module_id =
                    GetGLSLCodeModuleStableID(*definition);
                if (!GraphContainsModule(graph, module_id))
                    continue;

                for (uint32 k = 0;
                     k < definition->texture_requirement_count;
                     ++k)
                {
                    const GLSLCodeModuleTextureRequirement &requirement =
                        definition->texture_requirements[k];
                    if (requirement.slot == contract.texture_slot)
                        return module_id;
                }
            }

            return graph.modules.IsEmpty()
                ? 0
                : graph.modules[graph.modules.GetCount() - 1].module_id;
        }

        ShaderContractStableID FindSSBOReasonModule(
            const GLSLCodeModuleRegistry &registry,
            const ResolvedModuleGraph &graph,
            const ShaderDescriptorContractEntry &contract)
        {
            for (int i = 0; i < registry.GetCount(); ++i)
            {
                const GLSLCodeModuleDefinition *definition =
                    registry.GetModuleByIndex(i);
                if (!definition)
                    continue;

                const ShaderContractStableID module_id =
                    GetGLSLCodeModuleStableID(*definition);
                if (!GraphContainsModule(graph, module_id))
                    continue;

                for (uint32 k = 0;
                     k < definition->ssbo_requirement_count;
                     ++k)
                {
                    const GLSLCodeModuleSSBORequirement &requirement =
                        definition->ssbo_requirements[k];
                    if (requirement.data_slot == contract.data_slot
                     && requirement.ssbo_type == contract.ssbo_type)
                        return module_id;
                }
            }

            return graph.modules.IsEmpty()
                ? 0
                : graph.modules[graph.modules.GetCount() - 1].module_id;
        }

        int FindTextureBinding(
            const MaterialRecipe &recipe,
            const TextureSlot slot,
            ShadowResourcePlanDiagnostic &diagnostic)
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
                    SetPlanFailure(
                        diagnostic,
                        ShadowResourcePlanError::DuplicateRecipeTexture,
                        0,
                        slot);
                    return -2;
                }
                found = i;
            }
            return found;
        }

        int FindSSBOBinding(
            const MaterialRecipe &recipe,
            const uint32 data_slot,
            const SSBOType ssbo_type,
            ShadowResourcePlanDiagnostic &diagnostic)
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
                    SetPlanFailure(
                        diagnostic,
                        ShadowResourcePlanError::DuplicateRecipeSSBO,
                        0,
                        TextureSlot::BaseColor,
                        data_slot,
                        ssbo_type);
                    return -2;
                }
                found = i;
            }
            return found;
        }
    }

    const char *GetShadowResourcePlanErrorName(
        const ShadowResourcePlanError error) noexcept
    {
        switch (error)
        {
        case ShadowResourcePlanError::None: return "None";
        case ShadowResourcePlanError::InvalidEffectiveProgram: return "InvalidEffectiveProgram";
        case ShadowResourcePlanError::InvalidShaderInterface: return "InvalidShaderInterface";
        case ShadowResourcePlanError::DuplicateRecipeTexture: return "DuplicateRecipeTexture";
        case ShadowResourcePlanError::DuplicateRecipeSSBO: return "DuplicateRecipeSSBO";
        case ShadowResourcePlanError::DuplicatePlanResource: return "DuplicatePlanResource";
        case ShadowResourcePlanError::InvalidPlan: return "InvalidPlan";
        case ShadowResourcePlanError::MaterializationTextureMismatch: return "MaterializationTextureMismatch";
        case ShadowResourcePlanError::MaterializationStructMismatch: return "MaterializationStructMismatch";
        }
        return "Unknown";
    }

    bool BuildShadowResourceAcquirePlan(
        const MaterialRecipe &recipe,
        const GLSLCodeModuleRegistry &registry,
        const ResolvedModuleGraph &module_graph,
        const ShaderInterfaceContract &shader_interface,
        const EffectiveMaterialProgramKey &effective_program,
        ResourceAcquirePlan &out_plan,
        ShadowResourcePlanSummary &out_summary,
        ShadowResourcePlanDiagnostic &out_diagnostic)
    {
        out_plan = {};
        out_summary = {};
        out_diagnostic = {};

        const uint64 effective_digest = effective_program.GetDigest();
        if (effective_digest == 0)
            return SetPlanFailure(
                out_diagnostic,
                ShadowResourcePlanError::InvalidEffectiveProgram);
        if (!ValidateResolvedModuleGraph(module_graph)
         || !ValidateShaderInterfaceContract(shader_interface))
            return SetPlanFailure(
                out_diagnostic,
                ShadowResourcePlanError::InvalidShaderInterface);

        out_plan.effective_material_program_digest = effective_digest;
        std::vector<bool> used_textures(recipe.textures.size(), false);
        std::vector<bool> used_ssbos(recipe.ssbo_assets.size(), false);

        for (int i = 0;
             i < shader_interface.descriptor_requirements.GetCount();
             ++i)
        {
            const ShaderDescriptorContractEntry &contract =
                shader_interface.descriptor_requirements[i];

            const bool material_texture_contract =
                (contract.kind == DescriptorKind::Texture
                    && contract.semantic
                        == DescriptorSemantic::MaterialTexture)
             || (contract.kind == DescriptorKind::TextureSampler
                    && contract.semantic
                        == DescriptorSemantic::MaterialSampler);
            if (material_texture_contract)
            {
                const int binding_index = FindTextureBinding(
                    recipe, contract.texture_slot, out_diagnostic);
                if (binding_index == -2)
                    return false;
                if (binding_index < 0)
                {
                    if (contract.required && !contract.allow_fallback)
                        ++out_summary.missing_required_count;
                    else
                        ++out_summary.missing_optional_count;
                    continue;
                }

                const RecipeTextureBinding &binding =
                    recipe.textures[static_cast<size_t>(binding_index)];
                used_textures[static_cast<size_t>(binding_index)] = true;
                if (binding.use_direct_value)
                {
                    ++out_summary.direct_texture_value_count;
                    continue;
                }
                if (binding.resource_id.empty())
                {
                    if (contract.required || binding.required)
                        ++out_summary.missing_required_count;
                    else
                        ++out_summary.missing_optional_count;
                    continue;
                }

                ResourceAcquirePlanEntry entry{};
                entry.logical_resource_id = contract.logical_resource_id;
                entry.asset_identity_hash = HashText(binding.resource_id);
                entry.asset_metadata_hash =
                    HashTextureMetadata(contract, binding);
                entry.reason_contract_id =
                    FindTextureReasonModule(
                        registry, module_graph, contract);
                entry.reason_kind = ResourceAcquireReasonKind::Module;
                entry.semantic = contract.semantic;
                entry.texture_slot = contract.texture_slot;
                entry.kind = ResourceAcquireKind::Texture;
                entry.required = contract.required || binding.required;
                entry.allow_fallback = contract.allow_fallback;
                out_plan.resources.Add(entry);
                ++out_summary.planned_texture_count;
                continue;
            }

            if (contract.kind == DescriptorKind::SSBO
             && contract.semantic
                == DescriptorSemantic::MaterialDataSlotData)
            {
                const int binding_index = FindSSBOBinding(
                    recipe,
                    contract.data_slot,
                    contract.ssbo_type,
                    out_diagnostic);
                if (binding_index == -2)
                    return false;
                if (binding_index < 0)
                {
                    if (contract.required && !contract.allow_fallback)
                        ++out_summary.missing_required_count;
                    else
                        ++out_summary.missing_optional_count;
                    continue;
                }

                const RecipeSSBOAssetBinding &binding =
                    recipe.ssbo_assets[static_cast<size_t>(binding_index)];
                used_ssbos[static_cast<size_t>(binding_index)] = true;

                ResourceAcquirePlanEntry entry{};
                entry.logical_resource_id = contract.logical_resource_id;
                entry.asset_identity_hash =
                    HashSSBOAssetIdentity(binding);
                entry.asset_metadata_hash =
                    HashSSBOMetadata(contract, binding);
                entry.reason_contract_id =
                    FindSSBOReasonModule(
                        registry, module_graph, contract);
                entry.reason_kind = ResourceAcquireReasonKind::Module;
                entry.semantic = contract.semantic;
                entry.data_slot = contract.data_slot;
                entry.ssbo_type = contract.ssbo_type;
                entry.kind = ResourceAcquireKind::StorageBuffer;
                entry.required = contract.required;
                entry.allow_fallback = contract.allow_fallback;
                out_plan.resources.Add(entry);
                ++out_summary.planned_ssbo_count;
            }
        }

        for (const bool used : used_textures)
        {
            if (!used)
                ++out_summary.unused_recipe_texture_count;
        }
        for (const bool used : used_ssbos)
        {
            if (!used)
                ++out_summary.unused_recipe_ssbo_count;
        }

        if (!ValidateResourceAcquirePlan(out_plan))
            return SetPlanFailure(
                out_diagnostic,
                ShadowResourcePlanError::InvalidPlan);

        return true;
    }

    bool CompareShadowResourcePlanToMaterialization(
        const ResourceAcquirePlan &plan,
        const MaterializationSpec &materialization,
        ShadowResourcePlanDiagnostic &out_diagnostic) noexcept
    {
        out_diagnostic = {};
        if (!ValidateResourceAcquirePlan(plan))
            return SetPlanFailure(
                out_diagnostic,
                ShadowResourcePlanError::InvalidPlan);

        uint32 texture_count = 0;
        uint32 ssbo_count = 0;
        for (int i = 0; i < plan.resources.GetCount(); ++i)
        {
            const ResourceAcquirePlanEntry &entry = plan.resources[i];
            if (entry.kind == ResourceAcquireKind::Texture)
            {
                ++texture_count;
                bool found = false;
                for (const ResolvedResource &resource :
                     materialization.resources)
                {
                    if (resource.slot == entry.texture_slot)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    return SetPlanFailure(
                        out_diagnostic,
                        ShadowResourcePlanError::
                            MaterializationTextureMismatch,
                        entry.logical_resource_id,
                        entry.texture_slot);
            }
            else if (entry.kind == ResourceAcquireKind::StorageBuffer)
            {
                ++ssbo_count;
                bool found = false;
                for (const ResolvedStructRef &resource :
                     materialization.struct_refs)
                {
                    if (resource.data_slot == entry.data_slot
                     && resource.ssbo_type == entry.ssbo_type
                     && HashSSBOAssetIdentity(
                            resource.ssbo_type,
                            resource.ssbo_id,
                            resource.data_slot)
                            == entry.asset_identity_hash)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    return SetPlanFailure(
                        out_diagnostic,
                        ShadowResourcePlanError::
                            MaterializationStructMismatch,
                        entry.logical_resource_id,
                        TextureSlot::BaseColor,
                        entry.data_slot);
            }
        }

        if (texture_count != materialization.resources.size())
            return SetPlanFailure(
                out_diagnostic,
                ShadowResourcePlanError::MaterializationTextureMismatch);
        if (ssbo_count != materialization.struct_refs.size())
            return SetPlanFailure(
                out_diagnostic,
                ShadowResourcePlanError::MaterializationStructMismatch);
        return true;
    }
}
