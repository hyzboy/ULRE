#pragma once

#include <hgl/type/String.h>
#include <functional>

namespace hgl::graph
{
    class ShaderProgram;
    class ShaderCreateInfoMap;

    namespace mtl
    {
        class ShaderProgramBuildSpec;
    }

    enum class ShaderProgramCreatePrecheckDecision
    {
        Abort,
        UseCached,
        Proceed
    };

    struct ShaderProgramCreatePrecheckResult
    {
        ShaderProgram *cached_material = nullptr;
        const ShaderCreateInfoMap *shader_map = nullptr;
    };

    ShaderProgramCreatePrecheckDecision RunShaderProgramCreatePrecheck(const mtl::ShaderProgramBuildSpec *mci,
                                                             const AnsiString &material_name,
                                                             const std::function<ShaderProgram *(const AnsiString &)> &find_cached_material,
                                                             ShaderProgramCreatePrecheckResult &out_result);
}
