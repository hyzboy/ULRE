#pragma once

#include <hgl/graph/module/ShaderGenValidationTypes.h>
#include <string>

namespace hgl::graph
{
    namespace mtl::contract
    {
        struct ShaderGenContractValidationResult;
    }

    void AddShaderGenValidationWarning(ShaderGenValidationReport &report,
                                       const std::string &message);

    void AddShaderGenValidationError(ShaderGenValidationReport &report,
                                     const std::string &message);

    void MergeShaderGenValidationReport(ShaderGenValidationReport &dst,
                                        const ShaderGenValidationReport &src);

    void RecomputeShaderGenValidationOverallValid(ShaderGenValidationReport &report);

    void ApplyShaderGenContractValidationResult(ShaderGenValidationReport &report,
                                                const mtl::contract::ShaderGenContractValidationResult &contract_check,
                                                bool &valid_field);
}
