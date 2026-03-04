#include <hgl/graph/module/ShaderGenReadOnlyValidationGate.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <cstdio>

namespace hgl::graph
{
    bool RunReadOnlyValidationGate(const mtl::MaterialCreateInfo &mci,
                                   const mtl::contract::ShaderGenRequest *request_result,
                                   const mtl::contract::ShaderGenResult *mirror_result,
                                   const char *material_name,
                                   bool enable_mirror_validation,
                                   bool require_mirror_valid,
                                   RendererShaderGenAdapter::DiffLogDetail diff_log_detail)
    {
        if (!enable_mirror_validation)
            return true;

        const char *safe_material = material_name ? material_name : "<unnamed-material>";

        RendererShaderGenAdapter adapter;
        const RendererShaderGenAdapter::ValidationReport consume_report =
            adapter.ValidateMaterialContractReadOnly(mci,
                                                     request_result,
                                                     mirror_result,
                                                     safe_material,
                                                     diff_log_detail);

        if (!consume_report.overall_valid)
        {
            std::fprintf(stderr,
                "[RendererShaderGenAdapter] material=%s read-only validation failed (errors=%u, warnings=%u)\n",
                safe_material,
                consume_report.error_count,
                consume_report.warning_count);

            if (require_mirror_valid)
            {
                std::fprintf(stderr,
                    "[RendererShaderGenAdapter] material=%s creation aborted due to mirror-preferred strict mode\n",
                    safe_material);
                return false;
            }
        }

        return true;
    }
}
