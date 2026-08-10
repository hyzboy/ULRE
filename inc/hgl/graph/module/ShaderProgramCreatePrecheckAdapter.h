#pragma once

#include <hgl/type/String.h>

namespace hgl::graph
{
    class ShaderCreateInfoMap;

    namespace mtl
    {
        class ShaderProgramBuildSpec;
    }

    enum class ShaderProgramCreatePrecheckDecision
    {
        Abort,
        Proceed
    };

    struct ShaderProgramCreatePrecheckResult
    {
        const ShaderCreateInfoMap *shader_map = nullptr;
    };

    ShaderProgramCreatePrecheckDecision RunShaderProgramCreatePrecheck(const mtl::ShaderProgramBuildSpec *mci,
                                                             const AnsiString &material_name,
                                                             ShaderProgramCreatePrecheckResult &out_result);
}
