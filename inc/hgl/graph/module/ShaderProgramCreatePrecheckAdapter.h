#pragma once

#include <hgl/type/String.h>

namespace hgl::graph
{

    namespace shadergen
    {
        class ShaderCreateInfoMap;
        class ShaderProgramBuildSpec;
    }

    enum class ShaderProgramCreatePrecheckDecision
    {
        Abort,
        Proceed
    };

    struct ShaderProgramCreatePrecheckResult
    {
        const shadergen::ShaderCreateInfoMap *shader_map = nullptr;
    };

    ShaderProgramCreatePrecheckDecision RunShaderProgramCreatePrecheck(const shadergen::ShaderProgramBuildSpec *mci,
                                                             const AnsiString &material_name,
                                                             ShaderProgramCreatePrecheckResult &out_result);
}
