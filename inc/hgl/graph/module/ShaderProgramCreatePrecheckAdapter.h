#pragma once

#include <hgl/type/String.h>

namespace hgl::graph
{

    namespace mtl
    {
        class ShaderCreateInfoMap;
        class ShaderBuildContext;
    }

    enum class ShaderProgramCreatePrecheckDecision
    {
        Abort,
        Proceed
    };

    struct ShaderProgramCreatePrecheckResult
    {
        const mtl::ShaderCreateInfoMap *shader_map = nullptr;
    };

    ShaderProgramCreatePrecheckDecision RunShaderProgramCreatePrecheck(const mtl::ShaderBuildContext *ctx,
                                                             const AnsiString &material_name,
                                                             ShaderProgramCreatePrecheckResult &out_result);
}
