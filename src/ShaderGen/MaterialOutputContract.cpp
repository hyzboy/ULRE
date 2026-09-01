#include <hgl/mtl/MaterialOutputContract.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    namespace
    {
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
    }

    const char *GetMaterialOutputContractErrorName(
        const MaterialOutputContractError error) noexcept
    {
#define HGL_ERROR(name) case MaterialOutputContractError::name: return #name;
        switch (error)
        {
            HGL_MATERIAL_OUTPUT_CONTRACT_ERROR_LIST
        }
#undef HGL_ERROR
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
                    GetMaterialOutputStableID("outColor"),
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
}
