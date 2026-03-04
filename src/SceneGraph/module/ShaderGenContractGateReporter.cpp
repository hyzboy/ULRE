#include <hgl/graph/module/ShaderGenContractGateReporter.h>
#include <hgl/graph/module/RendererShaderGenAdapter.h>
#include <cstdio>

namespace hgl::graph
{
    static const char *SafeMaterialName(const char *material_name)
    {
        return material_name ? material_name : "<unnamed-material>";
    }

    static const char *SafeText(const char *text, const char *fallback)
    {
        return text ? text : fallback;
    }

    void ReportMirrorPreferredStrictAbort(const char *material_name,
                                          const char *category,
                                          const char *reason)
    {
        const char *safe_material = SafeMaterialName(material_name);
        const char *safe_reason = SafeText(reason, "<unknown>");
        const char *safe_category = SafeText(category, "StrictGate.Unknown");

        RendererShaderGenAdapter::RecordExternalValidationError(safe_material,
                                                                safe_reason,
                                                                safe_category);

        std::fprintf(stderr,
            "[RendererShaderGenAdapter] material=%s mirror-preferred build aborted: %s\n",
            safe_material,
            safe_reason);
    }

    void ReportMirrorSPVFallback(const char *material_name,
                                 const char *reason)
    {
        const char *safe_material = SafeMaterialName(material_name);
        const char *safe_reason = SafeText(reason, "<unknown>");

        std::fprintf(stderr,
            "[RendererShaderGenAdapter] material=%s mirror SPV build failed (%s), fallback to legacy path\n",
            safe_material,
            safe_reason);
    }

    void ReportMirrorVertexFallback(const char *material_name,
                                    const char *reason)
    {
        const char *safe_material = SafeMaterialName(material_name);
        const char *safe_reason = SafeText(reason, "<unknown>");

        std::fprintf(stderr,
            "[RendererShaderGenAdapter] material=%s mirror vertex fallback (%s), use legacy vertex input\n",
            safe_material,
            safe_reason);
    }

    void ReportMirrorDescriptorFallback(const char *material_name,
                                        const char *phase,
                                        const char *reason)
    {
        const char *safe_material = SafeMaterialName(material_name);
        const char *safe_phase = SafeText(phase, "layout build failed");
        const char *safe_reason = SafeText(reason, "<unknown>");

        std::fprintf(stderr,
            "[RendererShaderGenAdapter] material=%s mirror descriptor %s (%s), fallback to legacy descriptor layout\n",
            safe_material,
            safe_phase,
            safe_reason);
    }
}
