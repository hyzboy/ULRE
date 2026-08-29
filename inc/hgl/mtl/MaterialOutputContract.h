#pragma once

namespace hgl::graph::mtl {}

#include <hgl/mtl/PassType.h>
#include <hgl/mtl/CanonicalShaderContract.h>
#include <string>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    enum class MaterialOutputContractError : uint8
    {
        None = 0,
        UnsupportedPurpose,
        MissingContractMarker,
        UnsupportedAttachment,
        InvalidContract
    };

    struct MaterialOutputContractDiagnostic
    {
        MaterialOutputContractError error =
            MaterialOutputContractError::None;
        ShaderProgramPurpose purpose =
            ShaderProgramPurpose::ForwardColor;
        ShaderContractStableID attachment_semantic_id = 0;
    };

    const char *GetMaterialOutputContractErrorName(
        MaterialOutputContractError error) noexcept;

    ShaderProgramPurpose GetShaderProgramPurpose(
        PassType pass) noexcept;

    bool BuildMaterialOutputContract(
        ShaderProgramPurpose purpose,
        OutputContract &out_contract,
        MaterialOutputContractDiagnostic &out_diagnostic);

    bool BuildMaterialOutputContract(
        PassType pass,
        OutputContract &out_contract,
        MaterialOutputContractDiagnostic &out_diagnostic);

    bool ApplyMaterialOutputContract(
        const OutputContract &contract,
        const std::string &source,
        std::string &out_source,
        MaterialOutputContractDiagnostic &out_diagnostic);
}
