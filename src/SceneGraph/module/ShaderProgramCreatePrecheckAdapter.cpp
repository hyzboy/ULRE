#include <hgl/graph/module/ShaderProgramCreatePrecheckAdapter.h>
#include <hgl/shadergen/ShaderProgramBuildSpec.h>

namespace hgl::graph
{
    ShaderProgramCreatePrecheckDecision RunShaderProgramCreatePrecheck(const mtl::ShaderProgramBuildSpec *mci,
                                                             const AnsiString &material_name,
                                                             const std::function<ShaderProgram *(const AnsiString &)> &find_cached_material,
                                                             ShaderProgramCreatePrecheckResult &out_result)
    {
        out_result.cached_material = nullptr;
        out_result.shader_map = nullptr;

        if (!mci)
            return ShaderProgramCreatePrecheckDecision::Abort;

        if (find_cached_material)
        {
            out_result.cached_material = find_cached_material(material_name);
            if (out_result.cached_material)
                return ShaderProgramCreatePrecheckDecision::UseCached;
        }

        const ShaderCreateInfoMap &sci_map = mci->GetShaderMap();
        if (sci_map.GetCount() < 2)
            return ShaderProgramCreatePrecheckDecision::Abort;

        if (!mci->GetStageShader(ShaderStage::Fragment))
            return ShaderProgramCreatePrecheckDecision::Abort;

        out_result.shader_map = &sci_map;
        return ShaderProgramCreatePrecheckDecision::Proceed;
    }
}
