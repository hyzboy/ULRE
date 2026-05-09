#include <hgl/graph/module/MaterialCreatePrecheckAdapter.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/mtl/MaterialStagePolicy.h>
#include <cstdio>

namespace hgl::graph
{
    MaterialCreatePrecheckDecision RunMaterialCreatePrecheck(const mtl::MaterialCreateInfo *mci,
                                                             const AnsiString &material_name,
                                                             const std::function<ShaderMaterialProgram *(const AnsiString &)> &find_cached_material,
                                                             MaterialCreatePrecheckResult &out_result)
    {
        out_result.cached_material = nullptr;
        out_result.shader_map = nullptr;

        if (!mci)
        {
            std::fprintf(stderr,
                "[MaterialCreatePrecheck] Abort: MaterialCreateInfo is null for material '%s'\n",
                material_name.c_str());
            return MaterialCreatePrecheckDecision::Abort;
        }

        if (find_cached_material)
        {
            out_result.cached_material = find_cached_material(material_name);
            if (out_result.cached_material)
                return MaterialCreatePrecheckDecision::UseCached;
        }

        const uint32 stage_bits = mci->GetShaderStage();
        if (!mtl::IsSupportedMaterialShaderStageMask(stage_bits))
        {
            std::fprintf(stderr,
                "[MaterialCreatePrecheck] Abort: unsupported material shader stage mask=0x%08X for material '%s' (expected VS|FS only)\n",
                stage_bits,
                material_name.c_str());
            return MaterialCreatePrecheckDecision::Abort;
        }

        const ShaderStageMap &sci_map = mci->GetShaderMap();
        if (sci_map.GetCount() < 2)
        {
            std::fprintf(stderr,
                "[MaterialCreatePrecheck] Abort: shader map count=%d for material '%s' (expected >= 2)\n",
                sci_map.GetCount(),
                material_name.c_str());
            return MaterialCreatePrecheckDecision::Abort;
        }

        if (!mci->GetStageShader(ShaderStage::Vertex))
        {
            std::fprintf(stderr,
                "[MaterialCreatePrecheck] Abort: vertex shader missing for material '%s'\n",
                material_name.c_str());
            return MaterialCreatePrecheckDecision::Abort;
        }

        if (!mci->GetStageShader(ShaderStage::Fragment))
        {
            std::fprintf(stderr,
                "[MaterialCreatePrecheck] Abort: fragment shader missing for material '%s'\n",
                material_name.c_str());
            return MaterialCreatePrecheckDecision::Abort;
        }

        out_result.shader_map = &sci_map;
        return MaterialCreatePrecheckDecision::Proceed;
    }
}
