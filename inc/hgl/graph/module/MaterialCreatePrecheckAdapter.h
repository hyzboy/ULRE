#pragma once

#include <hgl/type/String.h>
#include <hgl/shadergen/ShaderCreateInfoMap.h>
#include <functional>

namespace hgl::graph
{
    class Material;

    namespace mtl
    {
        class MaterialCreateInfo;
    }

    enum class MaterialCreatePrecheckDecision
    {
        Abort,
        UseCached,
        Proceed
    };

    struct MaterialCreatePrecheckResult
    {
        Material *cached_material = nullptr;
        const ShaderCreateInfoMap *shader_map = nullptr;
    };

    MaterialCreatePrecheckDecision RunMaterialCreatePrecheck(const mtl::MaterialCreateInfo *mci,
                                                             const AnsiString &material_name,
                                                             const std::function<Material *(const AnsiString &)> &find_cached_material,
                                                             MaterialCreatePrecheckResult &out_result);
}
