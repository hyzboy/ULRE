#pragma once

namespace hgl::graph::mtl {}

#include <hgl/mtl/PassType.h>
#include <hgl/mtl/CanonicalShaderContract.h>
#include <string>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    // 契约错误 X 列表（单一真源——枚举与 GetXxxErrorName 同源，新增错误只改此处）
#define HGL_MATERIAL_OUTPUT_CONTRACT_ERROR_LIST \
    HGL_ERROR(None) \
    HGL_ERROR(UnsupportedPurpose) \
    HGL_ERROR(MissingContractMarker) \
    HGL_ERROR(UnsupportedAttachment) \
    HGL_ERROR(InvalidContract)

    enum class MaterialOutputContractError : uint8
    {
#define HGL_ERROR(name) name,
        HGL_MATERIAL_OUTPUT_CONTRACT_ERROR_LIST
#undef HGL_ERROR
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
