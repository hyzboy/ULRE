#include <hgl/graph/module/MaterialCreatePrecheckAdapter.h>
#include <hgl/shadergen/MaterialCreateInfo.h>

namespace hgl::graph
{
    MaterialCreatePrecheckDecision RunMaterialCreatePrecheck(const mtl::MaterialCreateInfo *mci,
                                                             const AnsiString &material_name,
                                                             const std::function<MaterialProgram *(const AnsiString &)> &find_cached_material,
                                                             MaterialCreatePrecheckResult &out_result)
    {
        out_result.cached_material = nullptr;
        out_result.shader_map = nullptr;

        if (!mci)
            return MaterialCreatePrecheckDecision::Abort;

        if (find_cached_material)
        {
            out_result.cached_material = find_cached_material(material_name);
            if (out_result.cached_material)
                return MaterialCreatePrecheckDecision::UseCached;
        }

        const ShaderCreateInfoMap &sci_map = mci->GetShaderMap();
        if (sci_map.GetCount() < 2)
            return MaterialCreatePrecheckDecision::Abort;

        if (!mci->GetStageShader(ShaderStage::Fragment))
            return MaterialCreatePrecheckDecision::Abort;

        out_result.shader_map = &sci_map;
        return MaterialCreatePrecheckDecision::Proceed;
    }
}
