#include <hgl/graph/module/ShaderProgramCreatePrecheckAdapter.h>
#include <hgl/shadergen/ShaderBuildContext.h>
#include <hgl/log/Log.h>

namespace hgl::graph
{
    ShaderProgramCreatePrecheckDecision RunShaderProgramCreatePrecheck(const shadergen::ShaderBuildContext *mci,
                                                             const AnsiString &material_name,
                                                             ShaderProgramCreatePrecheckResult &out_result)
    {
        out_result.shader_map = nullptr;

        if (!mci)
        {
            GLogError("[ShaderProgramPrecheck] null build spec: name=%s", material_name.c_str());
            return ShaderProgramCreatePrecheckDecision::Abort;
        }

        const shadergen::ShaderCreateInfoMap &sci_map = mci->GetShaderMap();
        if (sci_map.GetCount() < 2)
        {
            GLogError("[ShaderProgramPrecheck] incomplete shader map: name=%s count=%d",
                      material_name.c_str(), sci_map.GetCount());
            return ShaderProgramCreatePrecheckDecision::Abort;
        }

        if (!mci->GetStageShader(ShaderStage::Fragment))
        {
            GLogError("[ShaderProgramPrecheck] fragment stage missing: name=%s", material_name.c_str());
            return ShaderProgramCreatePrecheckDecision::Abort;
        }

        out_result.shader_map = &sci_map;
        return ShaderProgramCreatePrecheckDecision::Proceed;
    }
}
