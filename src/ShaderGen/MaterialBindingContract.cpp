#include <hgl/mtl/MaterialBindingContract.h>
#include <hgl/mtl/MaterialRecipe.h>

#include "common/CanonicalContractWriter.h"
#include <cstring>
#include <string_view>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::shadergen;
    namespace contract_detail = hgl::graph::shadergen::contract_detail;

    namespace
    {
        using contract_detail::CanonicalContractWriter;

        constexpr uint32 ResolvedBindingTableTag = 0x3156424Du;   // MBV1
        constexpr uint32 ResourceAcquirePlanTag = 0x31505152u;    // RQP1

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
                && binding.texture_slot >= TextureSlot::BEGIN_RANGE
                && binding.texture_slot <= TextureSlot::END_RANGE
                && IsValidTextureSource(binding.source)
                && ((binding.source == BindingSource::Asset
                  || binding.source == BindingSource::DirectValue)
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
                && binding.semantic == DescriptorSemantic::MaterialDataSlotData
                && binding.ssbo_type >= SSBOType::BEGIN_RANGE
                && binding.ssbo_type <= SSBOType::END_RANGE
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

        bool IsLess(
            const ResolvedTextureBinding &lhs,
            const ResolvedTextureBinding &rhs) noexcept
        {
            if (lhs.logical_resource_id != rhs.logical_resource_id)
                return lhs.logical_resource_id < rhs.logical_resource_id;
            if (lhs.texture_slot != rhs.texture_slot)
                return lhs.texture_slot < rhs.texture_slot;
            return lhs.recipe_binding_index < rhs.recipe_binding_index;
        }

        bool IsLess(
            const ResolvedDataBinding &lhs,
            const ResolvedDataBinding &rhs) noexcept
        {
            if (lhs.logical_resource_id != rhs.logical_resource_id)
                return lhs.logical_resource_id < rhs.logical_resource_id;
            if (lhs.data_slot != rhs.data_slot)
                return lhs.data_slot < rhs.data_slot;
            if (lhs.ssbo_type != rhs.ssbo_type)
                return lhs.ssbo_type < rhs.ssbo_type;
            return lhs.recipe_binding_index < rhs.recipe_binding_index;
        }

        bool IsLess(
            const ResourceAcquirePlanEntry &lhs,
            const ResourceAcquirePlanEntry &rhs) noexcept
        {
            if (lhs.logical_resource_id != rhs.logical_resource_id)
                return lhs.logical_resource_id < rhs.logical_resource_id;
            return static_cast<uint8>(lhs.kind)
                < static_cast<uint8>(rhs.kind);
        }

        void WriteResolvedTextureBinding(
            CanonicalContractWriter &writer,
            const ResolvedTextureBinding &binding)
        {
            writer.WriteU64(binding.logical_resource_id);
            writer.WriteU64(binding.asset_identity_hash);
            writer.WriteU64(binding.asset_metadata_hash);
            writer.WriteU16(static_cast<uint16>(binding.semantic));
            writer.WriteU16(static_cast<uint16>(binding.texture_slot));
            writer.WriteU32(binding.recipe_binding_index);
            writer.WriteU32(binding.direct_value);
            writer.WriteU8(static_cast<uint8>(binding.source));
            writer.WriteBool(binding.required);
            writer.WriteBool(binding.allow_fallback);
        }

        void WriteResolvedDataBinding(
            CanonicalContractWriter &writer,
            const ResolvedDataBinding &binding)
        {
            writer.WriteU64(binding.logical_resource_id);
            writer.WriteU64(binding.asset_identity_hash);
            writer.WriteU64(binding.asset_metadata_hash);
            writer.WriteU16(static_cast<uint16>(binding.semantic));
            writer.WriteU32(binding.data_slot);
            writer.WriteU32(binding.ssbo_id);
            writer.WriteU32(binding.data_index);
            writer.WriteU32(binding.recipe_binding_index);
            writer.WriteU16(static_cast<uint16>(binding.ssbo_type));
            writer.WriteU8(static_cast<uint8>(binding.source));
            writer.WriteBool(binding.use_data_index);
            writer.WriteBool(binding.shared_across_instances);
            writer.WriteBool(binding.required);
            writer.WriteBool(binding.allow_fallback);
        }
    }

    const char *GetBindingBuildErrorName(
        const BindingBuildError error) noexcept
    {
        switch (error)
        {
        case BindingBuildError::None: return "None";
        case BindingBuildError::InvalidShaderProgramKey:
            return "InvalidShaderProgramKey";
        case BindingBuildError::DuplicateRecipeTexture:
            return "DuplicateRecipeTexture";
        case BindingBuildError::DuplicateRecipeData:
            return "DuplicateRecipeData";
        case BindingBuildError::InvalidBindingTable:
            return "InvalidBindingTable";
        }
        return "Unknown";
    }

    const char *GetBindingSourceName(
        const BindingSource source) noexcept
    {
        switch (source)
        {
        case BindingSource::Asset: return "Asset";
        case BindingSource::DirectValue: return "DirectValue";
        case BindingSource::Missing: return "Missing";
        case BindingSource::Omitted: return "Omitted";
        }
        return "Unknown";
    }

    bool ValidateResolvedBindingTable(
        const ResolvedBindingTable &view) noexcept
    {
        if (view.schema_version != MaterialBindingContractSchemaVersion
         || view.program_key_digest == 0
         || view.source_binding_hash == 0)
            return false;

        uint32 observed_missing_required = 0;
        for (int i = 0; i < view.textures.GetCount(); ++i)
        {
            const ResolvedTextureBinding &binding = view.textures[i];
            if (!IsValidTextureBinding(binding))
                return false;

            if (binding.source == BindingSource::Missing)
                ++observed_missing_required;

            for (int j = 0; j < i; ++j)
            {
                if (view.textures[j].logical_resource_id == binding.logical_resource_id
                 || view.textures[j].texture_slot == binding.texture_slot)
                    return false;
            }
        }

        for (int i = 0; i < view.data.GetCount(); ++i)
        {
            const ResolvedDataBinding &binding = view.data[i];
            if (!IsValidDataBinding(binding))
                return false;

            if (binding.source == BindingSource::Missing)
                ++observed_missing_required;

            for (int j = 0; j < i; ++j)
            {
                if (view.data[j].logical_resource_id == binding.logical_resource_id
                 || (view.data[j].data_slot == binding.data_slot
                  && view.data[j].ssbo_type == binding.ssbo_type))
                    return false;
            }
        }

        return observed_missing_required == view.missing_required_count;
    }

    bool ValidateResourceAcquirePlan(
        const ResourceAcquirePlan &plan) noexcept
    {
        if (plan.schema_version != MaterialBindingContractSchemaVersion
         || plan.program_key_digest == 0)
            return false;

        for (int i = 0; i < plan.resources.GetCount(); ++i)
        {
            const ResourceAcquirePlanEntry &entry = plan.resources[i];
            if (entry.logical_resource_id == 0
             || entry.asset_identity_hash == 0
             || entry.asset_metadata_hash == 0
             || entry.semantic == DescriptorSemantic::Unknown
             || entry.kind < ResourceAcquireKind::Texture
             || entry.kind > ResourceAcquireKind::StorageBuffer)
                return false;

            if (entry.kind == ResourceAcquireKind::Texture)
            {
                if (entry.texture_slot < TextureSlot::BEGIN_RANGE
                 || entry.texture_slot > TextureSlot::END_RANGE
                 || (entry.semantic != DescriptorSemantic::MaterialTexture
                  && entry.semantic != DescriptorSemantic::MaterialSampler))
                    return false;
            }
            else if (entry.semantic != DescriptorSemantic::MaterialDataSlotData)
            {
                return false;
            }

            if (entry.kind == ResourceAcquireKind::StorageBuffer
             && (entry.ssbo_type < SSBOType::BEGIN_RANGE
              || entry.ssbo_type > SSBOType::END_RANGE))
                return false;

            for (int j = 0; j < i; ++j)
            {
                if (plan.resources[j].logical_resource_id
                    == entry.logical_resource_id)
                    return false;
            }
        }

        return true;
    }

    bool SerializeResolvedBindingTable(
        const ResolvedBindingTable &view,
        ValueArray<uint8> &out_bytes)
    {
        out_bytes.Clear();
        if (!ValidateResolvedBindingTable(view))
            return false;

        ValueArray<ResolvedTextureBinding> textures = view.textures;
        contract_detail::CanonicalSort(
            textures,
            [](const ResolvedTextureBinding &lhs,
               const ResolvedTextureBinding &rhs) noexcept
            {
                return IsLess(lhs, rhs);
            });
        ValueArray<ResolvedDataBinding> data = view.data;
        contract_detail::CanonicalSort(
            data,
            [](const ResolvedDataBinding &lhs,
               const ResolvedDataBinding &rhs) noexcept
            {
                return IsLess(lhs, rhs);
            });

        CanonicalContractWriter writer(out_bytes);
        writer.WriteU32(ResolvedBindingTableTag);
        writer.WriteU32(view.schema_version);
        writer.WriteU64(view.program_key_digest);
        writer.WriteU64(view.source_binding_hash);
        writer.WriteU32(view.missing_required_count);
        writer.WriteU32(view.unused_recipe_texture_count);
        writer.WriteU32(view.unused_recipe_data_count);
        writer.WriteU32(static_cast<uint32>(textures.GetCount()));
        for (int i = 0; i < textures.GetCount(); ++i)
            WriteResolvedTextureBinding(writer, textures[i]);
        writer.WriteU32(static_cast<uint32>(data.GetCount()));
        for (int i = 0; i < data.GetCount(); ++i)
            WriteResolvedDataBinding(writer, data[i]);
        return true;
    }

    bool SerializeResourceAcquirePlan(
        const ResourceAcquirePlan &plan,
        ValueArray<uint8> &out_bytes)
    {
        out_bytes.Clear();
        if (!ValidateResourceAcquirePlan(plan))
            return false;

        ValueArray<ResourceAcquirePlanEntry> resources = plan.resources;
        contract_detail::CanonicalSort(
            resources,
            [](const ResourceAcquirePlanEntry &lhs,
               const ResourceAcquirePlanEntry &rhs) noexcept
            {
                return IsLess(lhs, rhs);
            });

        CanonicalContractWriter writer(out_bytes);
        writer.WriteU32(ResourceAcquirePlanTag);
        writer.WriteU32(plan.schema_version);
        writer.WriteU64(plan.program_key_digest);
        writer.WriteU32(static_cast<uint32>(resources.GetCount()));
        for (int i = 0; i < resources.GetCount(); ++i)
        {
            const ResourceAcquirePlanEntry &entry = resources[i];
            writer.WriteU64(entry.logical_resource_id);
            writer.WriteU64(entry.asset_identity_hash);
            writer.WriteU64(entry.asset_metadata_hash);
            writer.WriteU8(static_cast<uint8>(entry.kind));
            writer.WriteU16(static_cast<uint16>(entry.semantic));
            writer.WriteU16(static_cast<uint16>(entry.texture_slot));
            writer.WriteU32(entry.data_slot);
            writer.WriteU16(static_cast<uint16>(entry.ssbo_type));
            writer.WriteBool(entry.required);
            writer.WriteBool(entry.allow_fallback);
        }
        return true;
    }

    uint64 GetBindingSourceHash(
        const MaterialRecipe &recipe) noexcept
    {
        hgl::hash::FNV1aHasher64 h;
        h << MaterialBindingContractSchemaVersion
          << static_cast<uint32>(recipe.textures.size());
        for (const auto &texture : recipe.textures)
        {
            h << texture.slot
              << texture.resource_id;
            h << texture.direct_value
              << texture.use_direct_value
              << texture.required;
        }

        h << static_cast<uint32>(recipe.ssbo_assets.size());
        for (const auto &binding : recipe.ssbo_assets)
        {
            h << binding.data_slot_name;
            h << binding.data_slot
              << binding.ssbo_type
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
        h << std::string_view(resource_id, resource_id_length);
        return h;
    }

    uint64 GetResolvedDataAssetIdentityHash(
        const SSBOType ssbo_type,
        const uint32 ssbo_id,
        const uint32 data_slot) noexcept
    {
        hgl::hash::FNV1aHasher64 h;
        h << ssbo_type
          << ssbo_id
          << data_slot;
        return h;
    }

    uint64 ResolvedBindingTable::GetStableHash() const noexcept
    {
        return GetResolvedBindingTableHash(*this);
    }

    bool ResolvedBindingTable::IsValid() const noexcept
    {
        return ValidateResolvedBindingTable(*this);
    }

    bool ResolvedBindingTable::IsRuntimeReady() const noexcept
    {
        return IsValid() && missing_required_count == 0;
    }

    uint64 GetResolvedBindingTableHash(
        const ResolvedBindingTable &view) noexcept
    {
        ValueArray<uint8> bytes;
        return SerializeResolvedBindingTable(view, bytes)
            ? contract_detail::HashCanonicalBytes(bytes) : 0;
    }

    uint64 GetResourceAcquirePlanHash(
        const ResourceAcquirePlan &plan) noexcept
    {
        ValueArray<uint8> bytes;
        return SerializeResourceAcquirePlan(plan, bytes)
            ? contract_detail::HashCanonicalBytes(bytes) : 0;
    }
}
