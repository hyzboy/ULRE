#include <hgl/shadergen/MaterialOutputContract.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        constexpr const char OutputContractMarker[] =
            "// ULRE_OUTPUT_CONTRACT";

        ShaderContractStableID GetOutputStableID(
            const char *name) noexcept
        {
            return name && name[0]
                ? hgl::hash::FNV1aAppendBytes(
                    hgl::hash::FNV1aInit<uint64>(),
                    name,
                    std::strlen(name))
                : 0;
        }

        bool SetFailure(
            MaterialOutputContractDiagnostic &diagnostic,
            const MaterialOutputContractError error,
            const ShaderProgramPurpose purpose,
            const ShaderContractStableID attachment_semantic_id = 0)
        {
            diagnostic.error = error;
            diagnostic.purpose = purpose;
            diagnostic.attachment_semantic_id =
                attachment_semantic_id;
            return false;
        }

        const char *GetOutputTypeName(
            const ShaderStageValueType value_type) noexcept
        {
            switch (value_type)
            {
            case ShaderStageValueType::Float: return "float";
            case ShaderStageValueType::Vec2: return "vec2";
            case ShaderStageValueType::Vec3: return "vec3";
            case ShaderStageValueType::Vec4: return "vec4";
            case ShaderStageValueType::Int: return "int";
            case ShaderStageValueType::UInt: return "uint";
            case ShaderStageValueType::Bool: return "bool";
            default: return nullptr;
            }
        }

        const char *GetOutputName(
            const ShaderContractStableID semantic_id) noexcept
        {
            if (semantic_id == GetOutputStableID("outColor"))
                return "outColor";
            return nullptr;
        }
    }

    const char *GetMaterialOutputContractErrorName(
        const MaterialOutputContractError error) noexcept
    {
        switch (error)
        {
        case MaterialOutputContractError::None: return "None";
        case MaterialOutputContractError::UnsupportedPurpose: return "UnsupportedPurpose";
        case MaterialOutputContractError::MissingContractMarker: return "MissingContractMarker";
        case MaterialOutputContractError::UnsupportedAttachment: return "UnsupportedAttachment";
        case MaterialOutputContractError::InvalidContract: return "InvalidContract";
        }
        return "Unknown";
    }

    ShaderProgramPurpose GetShaderProgramPurpose(
        const PassType pass) noexcept
    {
        switch (pass)
        {
        case PassType::ShadowOpaque:
        case PassType::ShadowMasked:
            return ShaderProgramPurpose::ShadowDepth;
        case PassType::EarlyZSolid:
        case PassType::EarlyZMasked:
            return ShaderProgramPurpose::DepthOnly;
        case PassType::VBufferID:
            return ShaderProgramPurpose::VBufferWrite;
        default:
            return ShaderProgramPurpose::ForwardColor;
        }
    }

    bool BuildMaterialOutputContract(
        const ShaderProgramPurpose purpose,
        OutputContract &out_contract,
        MaterialOutputContractDiagnostic &out_diagnostic)
    {
        out_contract = {};
        out_diagnostic = {};
        out_contract.purpose = purpose;

        switch (out_contract.purpose)
        {
        case ShaderProgramPurpose::ForwardColor:
            out_contract.attachments.Add(
                {
                    GetOutputStableID("outColor"),
                    ShaderStageValueType::Vec4,
                    0,
                    1,
                    0
                });
            break;
        case ShaderProgramPurpose::DepthOnly:
        case ShaderProgramPurpose::ShadowDepth:
            out_contract.depth_only = true;
            break;
        default:
            return SetFailure(
                out_diagnostic,
                MaterialOutputContractError::UnsupportedPurpose,
                out_contract.purpose);
        }

        if (!ValidateOutputContract(out_contract))
            return SetFailure(
                out_diagnostic,
                MaterialOutputContractError::InvalidContract,
                out_contract.purpose);
        return true;
    }

    bool BuildMaterialOutputContract(
        const PassType pass,
        OutputContract &out_contract,
        MaterialOutputContractDiagnostic &out_diagnostic)
    {
        return BuildMaterialOutputContract(
            GetShaderProgramPurpose(pass),
            out_contract,
            out_diagnostic);
    }

    bool ApplyMaterialOutputContract(
        const OutputContract &contract,
        const std::string &source,
        std::string &out_source,
        MaterialOutputContractDiagnostic &out_diagnostic)
    {
        out_source.clear();
        out_diagnostic = {};
        out_diagnostic.purpose = contract.purpose;
        if (!ValidateOutputContract(contract))
            return SetFailure(
                out_diagnostic,
                MaterialOutputContractError::InvalidContract,
                contract.purpose);

        const size_t marker = source.find(OutputContractMarker);
        if (marker == std::string::npos)
            return SetFailure(
                out_diagnostic,
                MaterialOutputContractError::MissingContractMarker,
                contract.purpose);

        std::string generated;
        for (int i = 0; i < contract.attachments.GetCount(); ++i)
        {
            const ShaderOutputAttachmentContract &attachment =
                contract.attachments[i];
            const char *type_name =
                GetOutputTypeName(attachment.value_type);
            const char *output_name =
                GetOutputName(attachment.write_semantic_id);
            if (!type_name || !output_name)
                return SetFailure(
                    out_diagnostic,
                    MaterialOutputContractError::UnsupportedAttachment,
                    contract.purpose,
                    attachment.write_semantic_id);

            generated += "layout(location=";
            generated += std::to_string(attachment.location);
            generated += ") out ";
            generated += type_name;
            generated += " ";
            generated += output_name;
            generated += ";\n";
            generated += "void WriteMaterialOutput(";
            generated += type_name;
            generated += " value) { ";
            generated += output_name;
            generated += " = value; }\n";
        }

        out_source.reserve(
            source.size() - sizeof(OutputContractMarker)
            + generated.size() + 1);
        out_source.append(source, 0, marker);
        out_source += generated;
        out_source.append(
            source,
            marker + sizeof(OutputContractMarker) - 1,
            std::string::npos);
        return true;
    }
}
