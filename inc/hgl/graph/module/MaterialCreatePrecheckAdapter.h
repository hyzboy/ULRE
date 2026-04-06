#pragma once

#include <hgl/type/String.h>
#include <functional>

namespace hgl::graph
{
    class MaterialTemplate;
    class ShaderStageMap;

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
        MaterialTemplate *cached_material = nullptr;
        const ShaderStageMap *shader_map = nullptr;
    };

    MaterialCreatePrecheckDecision RunMaterialCreatePrecheck(const mtl::MaterialCreateInfo *mci,
                                                             const AnsiString &material_name,
                                                             const std::function<MaterialTemplate *(const AnsiString &)> &find_cached_material,
                                                             MaterialCreatePrecheckResult &out_result);
}
