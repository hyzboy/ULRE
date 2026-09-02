#include <hgl/mtl/CanonicalShaderContract.h>

#include "contract/CanonicalContractWriter.h"

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    namespace
    {
        using contract_detail::CanonicalContractWriter;

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

    uint64 GetOutputContractHash(const OutputContract &contract) noexcept
    {
        ValueArray<uint8> bytes;
        return SerializeOutputContract(contract, bytes)
            ? contract_detail::HashCanonicalBytes(bytes) : 0;
    }

}
