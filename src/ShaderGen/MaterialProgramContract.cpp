#include <hgl/mtl/MaterialProgramContract.h>

#include "common/CanonicalContractWriter.h"

namespace hgl::graph::mtl
{
    namespace
    {
        using contract_detail::CanonicalContractWriter;

        constexpr uint32 EffectiveProgramTag = 0x314B5045u;   // EPK1
        constexpr uint32 SelectionRequestTag = 0x3152534Bu;   // KSR1
        constexpr uint32 ResolutionResultTag = 0x3152534Du;   // MSR1
        constexpr uint32 PreparedProgramSetTag = 0x31535050u; // PPS1
        constexpr uint32 ResourcePlanTag = 0x31504352u;       // RCP1

        void WriteEffectiveMaterialProgramKey(
            CanonicalContractWriter &writer,
            const EffectiveMaterialProgramKey &key)
        {
            writer.WriteU32(key.schema_version);
            writer.WriteU64(key.resolved_surface_profile_id);
            writer.WriteU64(key.resolved_surface_profile_hash);
            writer.WriteU64(key.projection_id);
            writer.WriteU64(key.normalized_static_feature_hash);
            writer.WriteU64(key.resolved_module_graph_hash);
            writer.WriteU64(key.capability_signature_hash);
            writer.WriteU64(key.resolver_policy_hash);
            writer.WriteU8(static_cast<uint8>(key.purpose));
        }

        bool IsLess(
            const EffectiveMaterialProgramKey &lhs,
            const EffectiveMaterialProgramKey &rhs) noexcept
        {
            if (lhs.resolved_surface_profile_id
                != rhs.resolved_surface_profile_id)
            {
                return lhs.resolved_surface_profile_id
                    < rhs.resolved_surface_profile_id;
            }
            if (lhs.projection_id != rhs.projection_id)
                return lhs.projection_id < rhs.projection_id;
            if (lhs.resolved_module_graph_hash
                != rhs.resolved_module_graph_hash)
            {
                return lhs.resolved_module_graph_hash
                    < rhs.resolved_module_graph_hash;
            }
            if (lhs.normalized_static_feature_hash
                != rhs.normalized_static_feature_hash)
            {
                return lhs.normalized_static_feature_hash
                    < rhs.normalized_static_feature_hash;
            }
            if (lhs.capability_signature_hash
                != rhs.capability_signature_hash)
            {
                return lhs.capability_signature_hash
                    < rhs.capability_signature_hash;
            }
            return static_cast<uint8>(lhs.purpose)
                < static_cast<uint8>(rhs.purpose);
        }
    }

    bool ValidateMaterialSelectionRequestKey(
        const MaterialSelectionRequestKey &key) noexcept
    {
        return key.schema_version == MaterialProgramContractSchemaVersion
            && key.definition_id_hash != 0
            && key.definition_content_hash != 0
            && key.recipe_capability_hash != 0
            && key.geometry_capability_hash != 0
            && key.device_quality_hash != 0
            && key.request_context_hash != 0
            && key.purpose >= ShaderProgramPurpose::ForwardColor
            && key.purpose <= ShaderProgramPurpose::PostProcess;
    }

    bool SerializeMaterialSelectionRequestKey(
        const MaterialSelectionRequestKey &key,
        ValueArray<uint8> &out_bytes)
    {
        out_bytes.Clear();
        if (!ValidateMaterialSelectionRequestKey(key))
            return false;

        CanonicalContractWriter writer(out_bytes);
        writer.WriteU32(SelectionRequestTag);
        writer.WriteU32(key.schema_version);
        writer.WriteU64(key.definition_id_hash);
        writer.WriteU64(key.definition_content_hash);
        writer.WriteU64(key.recipe_capability_hash);
        writer.WriteU64(key.geometry_capability_hash);
        writer.WriteU64(key.device_quality_hash);
        writer.WriteU64(key.request_context_hash);
        writer.WriteU8(static_cast<uint8>(key.purpose));
        writer.WriteU16(key.requested_material_lod);
        writer.WriteU16(key.requested_quality_class);
        return true;
    }

    uint64 MaterialSelectionRequestKey::GetDigest() const noexcept
    {
        ValueArray<uint8> bytes;
        return SerializeMaterialSelectionRequestKey(*this, bytes)
            ? contract_detail::HashCanonicalBytes(bytes) : 0;
    }

    bool ValidateEffectiveMaterialProgramKey(
        const EffectiveMaterialProgramKey &key) noexcept
    {
        return key.schema_version == MaterialProgramContractSchemaVersion
            && key.resolved_surface_profile_id != InvalidSurfaceProfileID
            && key.resolved_surface_profile_hash != 0
            && key.projection_id != InvalidSurfaceProjectionID
            && key.resolved_module_graph_hash != 0
            && key.capability_signature_hash != 0
            && key.resolver_policy_hash != 0
            && key.purpose >= ShaderProgramPurpose::ForwardColor
            && key.purpose <= ShaderProgramPurpose::PostProcess;
    }

    bool SerializeEffectiveMaterialProgramKey(
        const EffectiveMaterialProgramKey &key,
        ValueArray<uint8> &out_bytes)
    {
        out_bytes.Clear();
        if (!ValidateEffectiveMaterialProgramKey(key))
            return false;

        CanonicalContractWriter writer(out_bytes);
        writer.WriteU32(EffectiveProgramTag);
        WriteEffectiveMaterialProgramKey(writer, key);
        return true;
    }

    uint64 EffectiveMaterialProgramKey::GetDigest() const noexcept
    {
        ValueArray<uint8> bytes;
        return SerializeEffectiveMaterialProgramKey(*this, bytes)
            ? contract_detail::HashCanonicalBytes(bytes) : 0;
    }

    bool ValidateMaterialResolutionResult(
        const MaterialResolutionResult &result) noexcept
    {
        if (result.schema_version != MaterialProgramContractSchemaVersion)
            return false;

        switch (result.status)
        {
        case MaterialResolutionStatus::Unresolved:
            return result.effective_program.GetDigest() == 0;
        case MaterialResolutionStatus::Resolved:
            return result.source_definition_id_hash != 0
                && result.recipe_capability_hash != 0
                && result.source_surface_intent_id
                    != InvalidSurfaceIntentID
                && ValidateEffectiveMaterialProgramKey(
                    result.effective_program);
        case MaterialResolutionStatus::Failed:
            return result.source_definition_id_hash != 0
                && result.effective_program.GetDigest() == 0;
        }

        return false;
    }

    bool SerializeMaterialResolutionResult(
        const MaterialResolutionResult &result,
        ValueArray<uint8> &out_bytes)
    {
        out_bytes.Clear();
        if (!ValidateMaterialResolutionResult(result))
            return false;

        CanonicalContractWriter writer(out_bytes);
        writer.WriteU32(ResolutionResultTag);
        writer.WriteU32(result.schema_version);
        writer.WriteU8(static_cast<uint8>(result.status));
        writer.WriteU64(result.source_definition_id_hash);
        writer.WriteU64(result.recipe_capability_hash);
        writer.WriteU64(result.source_surface_intent_id);
        writer.WriteU16(result.requested_quality_class);
        writer.WriteU16(result.resolved_material_lod);
        writer.WriteU16(result.fallback_depth);

        const bool has_effective_program =
            result.status == MaterialResolutionStatus::Resolved;
        writer.WriteBool(has_effective_program);
        if (has_effective_program)
            WriteEffectiveMaterialProgramKey(
                writer, result.effective_program);
        return true;
    }

    uint64 MaterialResolutionResult::GetProvenanceHash() const noexcept
    {
        ValueArray<uint8> bytes;
        return SerializeMaterialResolutionResult(*this, bytes)
            ? contract_detail::HashCanonicalBytes(bytes) : 0;
    }

    bool PreparedMaterialProgramSet::IsValidForCurrentForwardPath()
        const noexcept
    {
        return schema_version == MaterialProgramContractSchemaVersion
            && maximum_surface_profile_id != InvalidSurfaceProfileID
            && quality_policy_hash != 0
            && programs.GetCount() == 1
            && ValidateEffectiveMaterialProgramKey(preferred_program)
            && programs[0] == preferred_program;
    }

    bool SerializePreparedMaterialProgramSet(
        const PreparedMaterialProgramSet &set,
        ValueArray<uint8> &out_bytes)
    {
        out_bytes.Clear();
        if (!set.IsValidForCurrentForwardPath())
            return false;

        ValueArray<EffectiveMaterialProgramKey> programs = set.programs;
        contract_detail::CanonicalSort(programs, IsLess);

        CanonicalContractWriter writer(out_bytes);
        writer.WriteU32(PreparedProgramSetTag);
        writer.WriteU32(set.schema_version);
        writer.WriteU64(set.maximum_surface_profile_id);
        writer.WriteU64(set.quality_policy_hash);
        WriteEffectiveMaterialProgramKey(writer, set.preferred_program);
        writer.WriteU32(static_cast<uint32>(programs.GetCount()));
        for (int i = 0; i < programs.GetCount(); ++i)
            WriteEffectiveMaterialProgramKey(writer, programs[i]);
        return true;
    }

    uint64 PreparedMaterialProgramSet::GetStableHash() const noexcept
    {
        ValueArray<uint8> bytes;
        return SerializePreparedMaterialProgramSet(*this, bytes)
            ? contract_detail::HashCanonicalBytes(bytes) : 0;
    }

    bool ValidateResourceAcquirePlan(
        const ResourceAcquirePlan &plan) noexcept
    {
        if (plan.schema_version != MaterialProgramContractSchemaVersion
         || plan.effective_material_program_digest == 0)
            return false;

        for (int i = 0; i < plan.resources.GetCount(); ++i)
        {
            const ResourceAcquirePlanEntry &entry = plan.resources[i];
            if (entry.logical_resource_id == 0
             || entry.asset_identity_hash == 0
             || entry.reason_module_id == 0
             || entry.semantic == DescriptorSemantic::Unknown)
                return false;
            if (entry.kind < ResourceAcquireKind::Texture
             || entry.kind > ResourceAcquireKind::Sampler
             || entry.texture_slot < TextureSlot::BEGIN_RANGE
             || entry.texture_slot > TextureSlot::END_RANGE
             || entry.ssbo_type < SSBOType::BEGIN_RANGE
             || entry.ssbo_type > SSBOType::END_RANGE)
                return false;

            for (int j = 0; j < i; ++j)
            {
                if (entry.logical_resource_id
                    == plan.resources[j].logical_resource_id)
                    return false;
            }
        }

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
               const ResourceAcquirePlanEntry &rhs)
            {
                if (lhs.logical_resource_id != rhs.logical_resource_id)
                    return lhs.logical_resource_id < rhs.logical_resource_id;
                return static_cast<uint8>(lhs.kind)
                    < static_cast<uint8>(rhs.kind);
            });

        CanonicalContractWriter writer(out_bytes);
        writer.WriteU32(ResourcePlanTag);
        writer.WriteU32(plan.schema_version);
        writer.WriteU64(plan.effective_material_program_digest);
        writer.WriteU32(static_cast<uint32>(resources.GetCount()));
        for (int i = 0; i < resources.GetCount(); ++i)
        {
            const ResourceAcquirePlanEntry &entry = resources[i];
            writer.WriteU64(entry.logical_resource_id);
            writer.WriteU64(entry.asset_identity_hash);
            writer.WriteU64(entry.asset_metadata_hash);
            writer.WriteU64(entry.reason_module_id);
            writer.WriteU8(static_cast<uint8>(entry.semantic));
            writer.WriteU8(static_cast<uint8>(entry.texture_slot));
            writer.WriteU32(entry.data_slot);
            writer.WriteU16(static_cast<uint16>(entry.ssbo_type));
            writer.WriteU8(static_cast<uint8>(entry.kind));
            writer.WriteBool(entry.required);
            writer.WriteBool(entry.allow_fallback);
        }

        return true;
    }

    uint64 GetResourceAcquirePlanHash(
        const ResourceAcquirePlan &plan) noexcept
    {
        ValueArray<uint8> bytes;
        return SerializeResourceAcquirePlan(plan, bytes)
            ? contract_detail::HashCanonicalBytes(bytes) : 0;
    }
}
