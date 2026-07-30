#pragma once

#include <hgl/type/String.h>
#include <functional>

namespace hgl::graph
{
    class MaterialProgram;
    class ShaderCreateInfoMap;

    namespace mtl
    {
        class ShaderProgramBuildSpec;
    }

    enum class MaterialCreatePrecheckDecision
    {
        Abort,
        UseCached,
        Proceed
    };

    struct MaterialCreatePrecheckResult
    {
        MaterialProgram *cached_material = nullptr;
        const ShaderCreateInfoMap *shader_map = nullptr;
    };

    MaterialCreatePrecheckDecision RunMaterialCreatePrecheck(const mtl::ShaderProgramBuildSpec *mci,
                                                             const AnsiString &material_name,
                                                             const std::function<MaterialProgram *(const AnsiString &)> &find_cached_material,
                                                             MaterialCreatePrecheckResult &out_result);
}
