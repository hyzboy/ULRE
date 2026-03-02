#pragma once

#include <hgl/shadergen/contract/ShaderGenContract.h>
#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl::contract
{
    struct ShaderGenContractValidationResult
    {
        bool valid = true;
        uint32_t warning_count = 0;
        uint32_t error_count = 0;

        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    ShaderGenContractValidationResult ValidateShaderGenRequestResult(const ShaderGenRequest &request,
                                                                     const ShaderGenResult &result,
                                                                     const char *material_name = nullptr);
}
