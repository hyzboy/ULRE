#include <hgl/mtl/MaterialBindingContract.h>
#include <hgl/mtl/MaterialRecipe.h>

#include "common/CanonicalContractWriter.h"
#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        using contract_detail::CanonicalContractWriter;

        constexpr uint32 MaterialBindingViewTag = 0x3156424Du;   // MBV1
        constexpr uint32 ResourceAcquirePlanTag = 0x31505152u;    // RQP1

        bool IsValidTextureSource(const MaterialBindingSource source) noexcept
        {
            return source >= MaterialBindingSource::Asset
                && source <= MaterialBindingSource::Omitted;
        }

        bool IsValidDataSource(const MaterialBindingSource source) noexcept
        {
            return source == MaterialBindingSource::Asset
                || source == MaterialBindingSource::Missing
                || source == MaterialBindingSource::Omitted;
        }

        bool IsValidTextureBinding(const MaterialTextureBinding &binding) noexcept
        {
            return binding.logical_resource_id != 0
                && binding.asset_identity_hash != 0
                && binding.asset_metadata_hash != 0
                && (binding.semantic == DescriptorSemantic::MaterialTexture
                 || binding.semantic == DescriptorSemantic::MaterialSampler)
                && binding.texture_slot >= TextureSlot::BEGIN_RANGE
                && binding.texture_slot <= TextureSlot::END_RANGE
                && IsValidTextureSource(binding.source)
                && ((binding.source == MaterialBindingSource::Asset
                  || binding.source == MaterialBindingSource::DirectValue)
                    ? binding.recipe_binding_index
                        != InvalidMaterialRecipeBindingIndex
                    : binding.recipe_binding_index
                        == InvalidMaterialRecipeBindingIndex)
                && !(binding.source == MaterialBindingSource::Missing
                 && !binding.required)
                && !(binding.source == MaterialBindingSource::Omitted
                 && binding.required);
        }

        bool IsValidDataBinding(const MaterialDataBinding &binding) noexcept
        {
            return binding.logical_resource_id != 0
                && binding.asset_identity_hash != 0
                && binding.asset_metadata_hash != 0
                && binding.semantic == DescriptorSemantic::MaterialDataSlotData
                && binding.ssbo_type >= SSBOType::BEGIN_RANGE
                && binding.ssbo_type <= SSBOType::END_RANGE
                && IsValidDataSource(binding.source)
                && ((binding.source == MaterialBindingSource::Asset)
                    ? binding.recipe_binding_index
                        != InvalidMaterialRecipeBindingIndex
                    : binding.recipe_binding_index
                        == InvalidMaterialRecipeBindingIndex)
                && !(binding.source == MaterialBindingSource::Missing
                 && !binding.required)
                && !(binding.source == MaterialBindingSource::Omitted
                 && binding.required);
        }

        uint64 AppendText(
            uint64 hash,
            const char *text) noexcept
        {
            const uint32 length = text ? static_cast<uint32>(strlen(text)) : 0;
            hash = hgl::hash::FNV1aAppendValueBytes(hash, length);
            return length > 0
                ? hgl::hash::FNV1aAppendBytes(hash, text, length)
                : hash;
        }

        uint64 HashTextureMetadata(
            const MaterialTextureBinding &binding) noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.logical_resource_id);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.semantic);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.texture_slot);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.recipe_binding_index);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.direct_value);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.source);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.required);
            return hgl::hash::FNV1aAppendValueBytes(hash, binding.allow_fallback);
        }

        uint64 HashDataMetadata(
            const MaterialDataBinding &binding) noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.logical_resource_id);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.semantic);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.data_slot);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.ssbo_id);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.data_index);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.ssbo_type);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.source);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.use_data_index);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.shared_across_instances);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.required);
            return hgl::hash::FNV1aAppendValueBytes(hash, binding.allow_fallback);
        }

        bool IsLess(
            const MaterialTextureBinding &lhs,
            const MaterialTextureBinding &rhs) noexcept
        {
            if (lhs.logical_resource_id != rhs.logical_resource_id)
                return lhs.logical_resource_id < rhs.logical_resource_id;
            if (lhs.texture_slot != rhs.texture_slot)
                return lhs.texture_slot < rhs.texture_slot;
            return lhs.recipe_binding_index < rhs.recipe_binding_index;
        }

        bool IsLess(
            const MaterialDataBinding &lhs,
            const MaterialDataBinding &rhs) noexcept
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

        void WriteMaterialTextureBinding(
            CanonicalContractWriter &writer,
            const MaterialTextureBinding &binding)
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

        void WriteMaterialDataBinding(
            CanonicalContractWriter &writer,
            const MaterialDataBinding &binding)
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

    const char *GetMaterialBindingViewBuildErrorName(
        const MaterialBindingViewBuildError error) noexcept
    {
        switch (error)
        {
        case MaterialBindingViewBuildError::None: return "None";
        case MaterialBindingViewBuildError::InvalidShaderProgramKey:
            return "InvalidShaderProgramKey";
        case MaterialBindingViewBuildError::DuplicateRecipeTexture:
            return "DuplicateRecipeTexture";
        case MaterialBindingViewBuildError::DuplicateRecipeData:
            return "DuplicateRecipeData";
        case MaterialBindingViewBuildError::InvalidBindingView:
            return "InvalidBindingView";
        }
        return "Unknown";
    }

    const char *GetMaterialBindingSourceName(
        const MaterialBindingSource source) noexcept
    {
        switch (source)
        {
        case MaterialBindingSource::Asset: return "Asset";
        case MaterialBindingSource::DirectValue: return "DirectValue";
        case MaterialBindingSource::Missing: return "Missing";
        case MaterialBindingSource::Omitted: return "Omitted";
        }
        return "Unknown";
    }

    bool ValidateMaterialBindingView(
        const MaterialBindingView &view) noexcept
    {
        if (view.schema_version != MaterialBindingContractSchemaVersion
         || view.program_key_digest == 0
         || view.source_binding_hash == 0)
            return false;

        uint32 observed_missing_required = 0;
        for (int i = 0; i < view.textures.GetCount(); ++i)
        {
            const MaterialTextureBinding &binding = view.textures[i];
            if (!IsValidTextureBinding(binding))
                return false;

            if (binding.source == MaterialBindingSource::Missing)
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
            const MaterialDataBinding &binding = view.data[i];
            if (!IsValidDataBinding(binding))
                return false;

            if (binding.source == MaterialBindingSource::Missing)
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

    bool ValidateMaterialResourceAcquirePlan(
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

    bool SerializeMaterialBindingView(
        const MaterialBindingView &view,
        ValueArray<uint8> &out_bytes)
    {
        out_bytes.Clear();
        if (!ValidateMaterialBindingView(view))
            return false;

        ValueArray<MaterialTextureBinding> textures = view.textures;
        contract_detail::CanonicalSort(
            textures,
            [](const MaterialTextureBinding &lhs,
               const MaterialTextureBinding &rhs) noexcept
            {
                return IsLess(lhs, rhs);
            });
        ValueArray<MaterialDataBinding> data = view.data;
        contract_detail::CanonicalSort(
            data,
            [](const MaterialDataBinding &lhs,
               const MaterialDataBinding &rhs) noexcept
            {
                return IsLess(lhs, rhs);
            });

        CanonicalContractWriter writer(out_bytes);
        writer.WriteU32(MaterialBindingViewTag);
        writer.WriteU32(view.schema_version);
        writer.WriteU64(view.program_key_digest);
        writer.WriteU64(view.source_binding_hash);
        writer.WriteU32(view.missing_required_count);
        writer.WriteU32(view.unused_recipe_texture_count);
        writer.WriteU32(view.unused_recipe_data_count);
        writer.WriteU32(static_cast<uint32>(textures.GetCount()));
        for (int i = 0; i < textures.GetCount(); ++i)
            WriteMaterialTextureBinding(writer, textures[i]);
        writer.WriteU32(static_cast<uint32>(data.GetCount()));
        for (int i = 0; i < data.GetCount(); ++i)
            WriteMaterialDataBinding(writer, data[i]);
        return true;
    }

    bool SerializeMaterialResourceAcquirePlan(
        const ResourceAcquirePlan &plan,
        ValueArray<uint8> &out_bytes)
    {
        out_bytes.Clear();
        if (!ValidateMaterialResourceAcquirePlan(plan))
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

    uint64 GetMaterialBindingSourceHash(
        const MaterialRecipe &recipe) noexcept
    {
        uint64 hash = hgl::hash::FNV1aInit<uint64>();
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, MaterialBindingContractSchemaVersion);

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, static_cast<uint32>(recipe.textures.size()));
        for (const auto &texture : recipe.textures)
        {
            hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.slot);
            hash = AppendText(hash, texture.resource_id.c_str());
            hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.direct_value);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.use_direct_value);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.required);
        }

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, static_cast<uint32>(recipe.ssbo_assets.size()));
        for (const auto &binding : recipe.ssbo_assets)
        {
            hash = AppendText(hash, binding.data_slot_name.c_str());
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.data_slot);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.ssbo_type);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.ssbo_id);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.data_index);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.use_data_index);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, binding.shared_across_instances);
        }
        return hash;
    }

    uint64 GetMaterialBindingTextureAssetIdentityHash(
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

    uint64 GetMaterialBindingDataAssetIdentityHash(
        const SSBOType ssbo_type,
        const uint32 ssbo_id,
        const uint32 data_slot) noexcept
    {
        uint64 hash = hgl::hash::FNV1aInit<uint64>();
        hash = hgl::hash::FNV1aAppendValueBytes(hash, ssbo_type);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, ssbo_id);
        return hgl::hash::FNV1aAppendValueBytes(hash, data_slot);
    }

    uint64 MaterialBindingView::GetStableHash() const noexcept
    {
        return GetMaterialBindingViewHash(*this);
    }

    bool MaterialBindingView::IsValid() const noexcept
    {
        return ValidateMaterialBindingView(*this);
    }

    bool MaterialBindingView::IsRuntimeReady() const noexcept
    {
        return IsValid() && missing_required_count == 0;
    }

    uint64 GetMaterialBindingViewHash(
        const MaterialBindingView &view) noexcept
    {
        ValueArray<uint8> bytes;
        return SerializeMaterialBindingView(view, bytes)
            ? contract_detail::HashCanonicalBytes(bytes) : 0;
    }

    uint64 GetMaterialResourceAcquirePlanHash(
        const ResourceAcquirePlan &plan) noexcept
    {
        ValueArray<uint8> bytes;
        return SerializeMaterialResourceAcquirePlan(plan, bytes)
            ? contract_detail::HashCanonicalBytes(bytes) : 0;
    }
}
