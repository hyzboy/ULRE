#pragma once

#include <hgl/CoreType.h>
#include <hgl/graph/ssbo/TextureSlot.h>
#include <hgl/mtl/DescriptorSemantic.h>
#include <hgl/mtl/SurfaceProfile.h>
#include <hgl/shadergen/CanonicalShaderContract.h>
#include <hgl/type/ValueArray.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    constexpr uint32 MaterialProgramContractSchemaVersion = 1;

    enum class MaterialResolutionStatus : uint8
    {
        Unresolved = 0,
        Resolved,
        Failed
    };

    struct MaterialSelectionRequestKey
    {
        uint32 schema_version = MaterialProgramContractSchemaVersion;
        uint64 definition_id_hash = 0;
        uint64 definition_content_hash = 0;
        uint64 recipe_capability_hash = 0;
        uint64 geometry_capability_hash = 0;
        uint64 device_quality_hash = 0;
        uint64 request_context_hash = 0;
        ShaderProgramPurpose purpose = ShaderProgramPurpose::ForwardColor;
        uint16 requested_material_lod = 0;
        uint16 requested_quality_class = 0;

        uint64 GetDigest() const noexcept;
    };

    inline bool operator==(
        const MaterialSelectionRequestKey &lhs,
        const MaterialSelectionRequestKey &rhs) noexcept
    {
        return lhs.schema_version == rhs.schema_version
            && lhs.definition_id_hash == rhs.definition_id_hash
            && lhs.definition_content_hash == rhs.definition_content_hash
            && lhs.recipe_capability_hash == rhs.recipe_capability_hash
            && lhs.geometry_capability_hash == rhs.geometry_capability_hash
            && lhs.device_quality_hash == rhs.device_quality_hash
            && lhs.request_context_hash == rhs.request_context_hash
            && lhs.purpose == rhs.purpose
            && lhs.requested_material_lod == rhs.requested_material_lod
            && lhs.requested_quality_class == rhs.requested_quality_class;
    }

    struct EffectiveMaterialProgramKey
    {
        uint32 schema_version = MaterialProgramContractSchemaVersion;
        SurfaceProfileID resolved_surface_profile_id =
            InvalidSurfaceProfileID;
        uint64 resolved_surface_profile_hash = 0;
        SurfaceProjectionID projection_id = InvalidSurfaceProjectionID;
        uint64 normalized_static_feature_hash = 0;
        uint64 resolved_module_graph_hash = 0;
        uint64 capability_signature_hash = 0;
        uint64 resolver_policy_hash = 0;
        ShaderProgramPurpose purpose = ShaderProgramPurpose::ForwardColor;

        uint64 GetDigest() const noexcept;
        AnsiString ToString() const
        {
            return AnsiString("effective-program-")
                + AnsiString::numberOf(GetDigest());
        }
    };

    inline bool operator==(
        const EffectiveMaterialProgramKey &lhs,
        const EffectiveMaterialProgramKey &rhs) noexcept
    {
        return lhs.schema_version == rhs.schema_version
            && lhs.resolved_surface_profile_id
                == rhs.resolved_surface_profile_id
            && lhs.resolved_surface_profile_hash
                == rhs.resolved_surface_profile_hash
            && lhs.projection_id == rhs.projection_id
            && lhs.normalized_static_feature_hash
                == rhs.normalized_static_feature_hash
            && lhs.resolved_module_graph_hash
                == rhs.resolved_module_graph_hash
            && lhs.capability_signature_hash
                == rhs.capability_signature_hash
            && lhs.resolver_policy_hash == rhs.resolver_policy_hash
            && lhs.purpose == rhs.purpose;
    }

    struct MaterialResolutionResult
    {
        uint32 schema_version = MaterialProgramContractSchemaVersion;
        MaterialResolutionStatus status =
            MaterialResolutionStatus::Unresolved;
        uint64 source_definition_id_hash = 0;
        uint64 recipe_capability_hash = 0;
        SurfaceIntentID source_surface_intent_id =
            InvalidSurfaceIntentID;
        uint16 requested_quality_class = 0;
        uint16 resolved_material_lod = 0;
        uint16 fallback_depth = 0;
        EffectiveMaterialProgramKey effective_program;

        uint64 GetProvenanceHash() const noexcept;
    };

    struct PreparedMaterialProgramSet
    {
        uint32 schema_version = MaterialProgramContractSchemaVersion;
        SurfaceProfileID maximum_surface_profile_id =
            InvalidSurfaceProfileID;
        uint64 quality_policy_hash = 0;
        EffectiveMaterialProgramKey preferred_program;
        ValueArray<EffectiveMaterialProgramKey> programs;

        bool IsValidForCurrentForwardPath() const noexcept;
        uint64 GetStableHash() const noexcept;
    };

    enum class ResourceAcquireKind : uint8
    {
        Texture = 0,
        UniformBuffer,
        StorageBuffer,
        Sampler
    };

    struct ResourceAcquirePlanEntry
    {
        uint64 logical_resource_id = 0;
        uint64 asset_identity_hash = 0;
        uint64 asset_metadata_hash = 0;
        ShaderContractStableID reason_module_id = 0;
        DescriptorSemantic semantic = DescriptorSemantic::Unknown;
        TextureSlot texture_slot = TextureSlot::BaseColor;
        uint32 data_slot = 0;
        SSBOType ssbo_type = SSBOType::UserDefined;
        ResourceAcquireKind kind = ResourceAcquireKind::Texture;
        bool required = true;
        bool allow_fallback = false;
    };

    inline bool operator==(
        const ResourceAcquirePlanEntry &lhs,
        const ResourceAcquirePlanEntry &rhs) noexcept
    {
        return lhs.logical_resource_id == rhs.logical_resource_id
            && lhs.asset_identity_hash == rhs.asset_identity_hash
            && lhs.asset_metadata_hash == rhs.asset_metadata_hash
            && lhs.reason_module_id == rhs.reason_module_id
            && lhs.semantic == rhs.semantic
            && lhs.texture_slot == rhs.texture_slot
            && lhs.data_slot == rhs.data_slot
            && lhs.ssbo_type == rhs.ssbo_type
            && lhs.kind == rhs.kind
            && lhs.required == rhs.required
            && lhs.allow_fallback == rhs.allow_fallback;
    }

    struct ResourceAcquirePlan
    {
        uint32 schema_version = MaterialProgramContractSchemaVersion;
        uint64 effective_material_program_digest = 0;
        ValueArray<ResourceAcquirePlanEntry> resources;
    };

    bool ValidateEffectiveMaterialProgramKey(
        const EffectiveMaterialProgramKey &key) noexcept;
    bool ValidateMaterialSelectionRequestKey(
        const MaterialSelectionRequestKey &key) noexcept;
    bool ValidateMaterialResolutionResult(
        const MaterialResolutionResult &result) noexcept;
    bool ValidateResourceAcquirePlan(
        const ResourceAcquirePlan &plan) noexcept;

    bool SerializeEffectiveMaterialProgramKey(
        const EffectiveMaterialProgramKey &key,
        ValueArray<uint8> &out_bytes);
    bool SerializeMaterialSelectionRequestKey(
        const MaterialSelectionRequestKey &key,
        ValueArray<uint8> &out_bytes);
    bool SerializeMaterialResolutionResult(
        const MaterialResolutionResult &result,
        ValueArray<uint8> &out_bytes);
    bool SerializePreparedMaterialProgramSet(
        const PreparedMaterialProgramSet &set,
        ValueArray<uint8> &out_bytes);
    bool SerializeResourceAcquirePlan(
        const ResourceAcquirePlan &plan,
        ValueArray<uint8> &out_bytes);

    uint64 GetResourceAcquirePlanHash(
        const ResourceAcquirePlan &plan) noexcept;
}
