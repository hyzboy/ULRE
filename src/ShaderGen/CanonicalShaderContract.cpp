#include <hgl/mtl/CanonicalShaderContract.h>

#include "common/CanonicalContractWriter.h"

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    namespace
    {
        using contract_detail::CanonicalContractWriter;

        constexpr uint32 ShaderInterfaceTag = 0x31494653u;     // SFI1
        constexpr uint32 OutputContractTag = 0x3154554Fu;      // OUT1

        bool HasLocationConflict(
            const uint32 first_location,
            const uint32 first_width,
            const uint32 second_location,
            const uint32 second_width) noexcept
        {
            return first_location < second_location + second_width
                && second_location < first_location + first_width;
        }

        bool IsValidShape(
            const ShaderSemanticScalarType scalar_type,
            const uint8 component_count,
            const uint8 location_width) noexcept
        {
            return scalar_type != ShaderSemanticScalarType::Unknown
                && component_count > 0
                && component_count <= 4
                && location_width > 0;
        }

        bool IsValidPurpose(const ShaderProgramPurpose purpose) noexcept
        {
            switch (purpose)
            {
            case ShaderProgramPurpose::ForwardColor:
            case ShaderProgramPurpose::DepthOnly:
            case ShaderProgramPurpose::ShadowDepth:
                return true;
            default:
                return false;
            }
        }

        bool IsSingleShaderStage(const ShaderStage stage) noexcept
        {
            switch (stage)
            {
            case ShaderStage::Fragment:
            case ShaderStage::Compute:
            case ShaderStage::Task:
            case ShaderStage::Mesh:
            case ShaderStage::ClusterCulling:
                return true;
            default:
                return false;
            }
        }
    }

    bool ValidateShaderInterfaceContract(
        const ShaderInterfaceContract &contract) noexcept
    {

        for (int i = 0; i < contract.geometry_semantics.GetCount(); ++i)
        {
            const GeometrySemanticContractEntry &entry =
                contract.geometry_semantics[i];
            if (entry.semantic == VertexSemantic::Unknown
             || entry.physical_location == InvalidShaderSemanticLocation
             || entry.physical_format == 0
             || !IsValidShape(
                    entry.scalar_type,
                    entry.component_count,
                    entry.location_width))
                return false;

            for (int j = 0; j < i; ++j)
            {
                const GeometrySemanticContractEntry &other =
                    contract.geometry_semantics[j];
                if (entry.semantic == other.semantic
                 || HasLocationConflict(
                        entry.physical_location,
                        entry.location_width,
                        other.physical_location,
                        other.location_width))
                    return false;
            }
        }

        for (int i = 0; i < contract.inter_stage_semantics.GetCount(); ++i)
        {
            const InterStageSemanticContractEntry &entry =
                contract.inter_stage_semantics[i];
            if (entry.semantic == InterStageSemantic::Unknown
             || entry.location == InvalidShaderSemanticLocation
             || entry.interpolation < InterStageInterpolation::Smooth
             || entry.interpolation > InterStageInterpolation::NoPerspective
             || !IsValidShape(
                    entry.scalar_type,
                    entry.component_count,
                    entry.location_width))
                return false;

            for (int j = 0; j < i; ++j)
            {
                const InterStageSemanticContractEntry &other =
                    contract.inter_stage_semantics[j];
                if (entry.semantic == other.semantic
                 || HasLocationConflict(
                        entry.location,
                        entry.location_width,
                        other.location,
                        other.location_width))
                    return false;
            }
        }

        for (int i = 0; i < contract.descriptor_requirements.GetCount(); ++i)
        {
            const ShaderDescriptorContractEntry &entry =
                contract.descriptor_requirements[i];
            if (entry.logical_resource_id == 0
             || entry.semantic == DescriptorSemantic::Unknown
             || entry.semantic_layer == DescriptorSemanticLayer::Unknown
             || entry.semantic_layer > DescriptorSemanticLayer::Sampler
             || entry.set_type == DescriptorSetType::Unknown
             || entry.set_type < DescriptorSetType::Scene
             || entry.set_type > DescriptorSetType::Vertex   // Phase 5：Vertex 为最后一个集合类型
             || entry.texture_slot < TextureSlot::BEGIN_RANGE
             || entry.texture_slot > TextureSlot::END_RANGE
             || entry.ssbo_type < SSBOType::BEGIN_RANGE
             || entry.ssbo_type > SSBOType::END_RANGE
             || entry.array_count == 0
             || entry.stage_flags == 0)
                return false;

            for (int j = 0; j < i; ++j)
            {
                if (entry.logical_resource_id
                    == contract.descriptor_requirements[j].
                        logical_resource_id)
                    return false;
            }
        }

        for (int i = 0; i < contract.entry_points.GetCount(); ++i)
        {
            const ShaderEntryPointContract &entry = contract.entry_points[i];
            if (entry.entry_point_id == 0
             || !IsSingleShaderStage(entry.stage))
                return false;

            for (int j = 0; j < i; ++j)
            {
                if (entry.stage == contract.entry_points[j].stage)
                    return false;
            }
        }

        return true;
    }

    bool ValidateOutputContract(const OutputContract &contract) noexcept
    {
        if (!IsValidPurpose(contract.purpose))
            return false;
        if (contract.depth_only)
            return contract.attachments.IsEmpty();
        if (contract.attachments.IsEmpty())
            return false;

        for (int i = 0; i < contract.attachments.GetCount(); ++i)
        {
            const ShaderOutputAttachmentContract &entry =
                contract.attachments[i];
            if (entry.write_semantic_id == 0
             || entry.value_type == ShaderStageValueType::Unknown
             || entry.value_type > ShaderStageValueType::Bool
             || entry.location_width == 0)
                return false;

            for (int j = 0; j < i; ++j)
            {
                const ShaderOutputAttachmentContract &other =
                    contract.attachments[j];
                if (entry.write_semantic_id == other.write_semantic_id
                 || HasLocationConflict(
                        entry.location,
                        entry.location_width,
                        other.location,
                        other.location_width))
                    return false;
            }
        }

        return true;
    }

    bool SerializeShaderInterfaceContract(
        const ShaderInterfaceContract &contract,
        ValueArray<uint8> &out_bytes)
    {
        out_bytes.Clear();
        if (!ValidateShaderInterfaceContract(contract))
            return false;

        ValueArray<GeometrySemanticContractEntry> geometry =
            contract.geometry_semantics;
        contract_detail::CanonicalSort(
            geometry,
            [](const GeometrySemanticContractEntry &lhs,
               const GeometrySemanticContractEntry &rhs)
            {
                return static_cast<uint8>(lhs.semantic)
                    < static_cast<uint8>(rhs.semantic);
            });

        ValueArray<InterStageSemanticContractEntry> inter_stage =
            contract.inter_stage_semantics;
        contract_detail::CanonicalSort(
            inter_stage,
            [](const InterStageSemanticContractEntry &lhs,
               const InterStageSemanticContractEntry &rhs)
            {
                return static_cast<uint8>(lhs.semantic)
                    < static_cast<uint8>(rhs.semantic);
            });

        ValueArray<ShaderDescriptorContractEntry> descriptors =
            contract.descriptor_requirements;
        contract_detail::CanonicalSort(
            descriptors,
            [](const ShaderDescriptorContractEntry &lhs,
               const ShaderDescriptorContractEntry &rhs)
            {
                return lhs.logical_resource_id < rhs.logical_resource_id;
            });

        ValueArray<ShaderEntryPointContract> entry_points =
            contract.entry_points;
        contract_detail::CanonicalSort(
            entry_points,
            [](const ShaderEntryPointContract &lhs,
               const ShaderEntryPointContract &rhs)
            {
                return static_cast<uint32>(lhs.stage)
                    < static_cast<uint32>(rhs.stage);
            });

        CanonicalContractWriter writer(out_bytes);
        writer.WriteU32(ShaderInterfaceTag);

        writer.WriteU32(static_cast<uint32>(geometry.GetCount()));
        for (int i = 0; i < geometry.GetCount(); ++i)
        {
            writer.WriteU8(static_cast<uint8>(geometry[i].semantic));
            writer.WriteU8(static_cast<uint8>(geometry[i].scalar_type));
            writer.WriteU8(geometry[i].component_count);
            writer.WriteU8(geometry[i].location_width);
            writer.WriteU32(geometry[i].physical_location);
            writer.WriteU32(geometry[i].physical_format);
        }

        writer.WriteU32(static_cast<uint32>(inter_stage.GetCount()));
        for (int i = 0; i < inter_stage.GetCount(); ++i)
        {
            writer.WriteU8(static_cast<uint8>(inter_stage[i].semantic));
            writer.WriteU8(static_cast<uint8>(inter_stage[i].scalar_type));
            writer.WriteU8(static_cast<uint8>(inter_stage[i].interpolation));
            writer.WriteU8(inter_stage[i].component_count);
            writer.WriteU8(inter_stage[i].location_width);
            writer.WriteU32(inter_stage[i].location);
        }

        writer.WriteU32(static_cast<uint32>(descriptors.GetCount()));
        for (int i = 0; i < descriptors.GetCount(); ++i)
        {
            const ShaderDescriptorContractEntry &entry = descriptors[i];
            writer.WriteU64(entry.logical_resource_id);
            writer.WriteU64(entry.resource_schema_id);
            writer.WriteU8(static_cast<uint8>(entry.semantic));
            writer.WriteU8(static_cast<uint8>(entry.semantic_layer));
            writer.WriteI32(static_cast<int32>(entry.set_type));
            writer.WriteU8(static_cast<uint8>(entry.texture_slot));
            writer.WriteU16(static_cast<uint16>(entry.ssbo_type));
            writer.WriteU32(entry.material_private_data_slot);
            writer.WriteU32(entry.stage_flags);
            writer.WriteU32(entry.array_count);
            writer.WriteBool(entry.required);
            writer.WriteBool(entry.allow_fallback);
        }

        writer.WriteU32(static_cast<uint32>(entry_points.GetCount()));
        for (int i = 0; i < entry_points.GetCount(); ++i)
        {
            writer.WriteU32(static_cast<uint32>(entry_points[i].stage));
            writer.WriteU64(entry_points[i].entry_point_id);
        }

        return true;
    }

    bool SerializeOutputContract(
        const OutputContract &contract,
        ValueArray<uint8> &out_bytes)
    {
        out_bytes.Clear();
        if (!ValidateOutputContract(contract))
            return false;

        ValueArray<ShaderOutputAttachmentContract> attachments =
            contract.attachments;
        contract_detail::CanonicalSort(
            attachments,
            [](const ShaderOutputAttachmentContract &lhs,
               const ShaderOutputAttachmentContract &rhs)
            {
                if (lhs.location != rhs.location)
                    return lhs.location < rhs.location;
                return lhs.write_semantic_id < rhs.write_semantic_id;
            });

        CanonicalContractWriter writer(out_bytes);
        writer.WriteU32(OutputContractTag);
                writer.WriteU8(static_cast<uint8>(contract.purpose));
        writer.WriteBool(contract.depth_only);
        writer.WriteU32(static_cast<uint32>(attachments.GetCount()));
        for (int i = 0; i < attachments.GetCount(); ++i)
        {
            writer.WriteU64(attachments[i].write_semantic_id);
            writer.WriteU32(
                static_cast<uint32>(attachments[i].value_type));
            writer.WriteU32(attachments[i].location);
            writer.WriteU32(attachments[i].location_width);
            writer.WriteU32(attachments[i].flags);
        }

        return true;
    }

    uint64 GetShaderInterfaceContractHash(
        const ShaderInterfaceContract &contract) noexcept
    {
        ValueArray<uint8> bytes;
        return SerializeShaderInterfaceContract(contract, bytes)
            ? contract_detail::HashCanonicalBytes(bytes) : 0;
    }

    uint64 GetOutputContractHash(const OutputContract &contract) noexcept
    {
        ValueArray<uint8> bytes;
        return SerializeOutputContract(contract, bytes)
            ? contract_detail::HashCanonicalBytes(bytes) : 0;
    }

}
