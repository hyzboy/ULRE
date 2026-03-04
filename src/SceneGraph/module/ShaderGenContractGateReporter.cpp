#include <hgl/graph/module/ShaderGenContractGateReporter.h>
#include <hgl/graph/module/ShaderGenValidationStorageService.h>
#include <atomic>
#include <cstdarg>
#include <cstdio>

namespace hgl::graph
{
    namespace
    {
        const char *SafeMaterialName(const char *material_name)
        {
            return material_name ? material_name : "<unnamed-material>";
        }

        const char *SafeText(const char *text, const char *fallback)
        {
            return text ? text : fallback;
        }

        void DefaultShaderGenContractGateLogSink(const char *message)
        {
            if (!message)
                return;

            std::fputs(message, stderr);
        }

        std::atomic<ShaderGenContractGateLogSink> &GetShaderGenContractGateLogSinkRef()
        {
            static std::atomic<ShaderGenContractGateLogSink> sink(DefaultShaderGenContractGateLogSink);
            return sink;
        }

        void EmitShaderGenContractGateLog(const char *format, ...)
        {
            if (!format)
                return;

            char buffer[512] = {};

            va_list args;
            va_start(args, format);
            std::vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);

            ShaderGenContractGateLogSink sink = GetShaderGenContractGateLogSinkRef().load();
            if (!sink)
                sink = DefaultShaderGenContractGateLogSink;

            sink(buffer);
        }

        void ReportMirrorFallbackWithReason(const char *material_name,
                                            const char *reason,
                                            const char *format)
        {
            const char *safe_material = SafeMaterialName(material_name);
            const char *safe_reason = SafeText(reason, "<unknown>");

            EmitShaderGenContractGateLog(format,
                                         safe_material,
                                         safe_reason);
        }

        void ReportMirrorFallbackWithPhaseReason(const char *material_name,
                                                 const char *phase,
                                                 const char *reason,
                                                 const char *phase_fallback,
                                                 const char *format)
        {
            const char *safe_material = SafeMaterialName(material_name);
            const char *safe_phase = SafeText(phase, phase_fallback);
            const char *safe_reason = SafeText(reason, "<unknown>");

            EmitShaderGenContractGateLog(format,
                                         safe_material,
                                         safe_phase,
                                         safe_reason);
        }
    }//namespace

    std::string BuildMirrorPreferredAbortReason(const char *reason)
    {
        const char *safe_reason = SafeText(reason, "<unknown>");

        std::string message;
        message.reserve(std::char_traits<char>::length(kShaderGenMirrorPreferredAbortPrefix)
                      + std::char_traits<char>::length(safe_reason));
        message += kShaderGenMirrorPreferredAbortPrefix;
        message += safe_reason;
        return message;
    }

    void SetShaderGenContractGateLogSink(ShaderGenContractGateLogSink sink)
    {
        GetShaderGenContractGateLogSinkRef().store(sink ? sink : DefaultShaderGenContractGateLogSink);
    }

    void ResetShaderGenContractGateLogSink()
    {
        GetShaderGenContractGateLogSinkRef().store(DefaultShaderGenContractGateLogSink);
    }

    void ReportMirrorPreferredStrictAbort(const char *material_name,
                                          const char *category,
                                          const char *reason)
    {
        const char *safe_material = SafeMaterialName(material_name);
        const char *safe_reason = SafeText(reason, "<unknown>");
        const char *safe_category = SafeText(category, kShaderGenStrictGateUnknownCategory);

        StoreExternalShaderGenValidationError(safe_material,
                              safe_reason,
                              safe_category);

        EmitShaderGenContractGateLog(
            "[RendererShaderGenAdapter] material=%s mirror-preferred build aborted: %s\n",
            safe_material,
            safe_reason);
    }

    void ReportMirrorSPVFallback(const char *material_name,
                                 const char *reason)
    {
        ReportMirrorFallbackWithReason(
            material_name,
            reason,
            "[RendererShaderGenAdapter] material=%s mirror SPV build failed (%s), fallback to legacy path\n");
    }

    void ReportMirrorVertexFallback(const char *material_name,
                                    const char *reason)
    {
        ReportMirrorFallbackWithReason(
            material_name,
            reason,
            "[RendererShaderGenAdapter] material=%s mirror vertex fallback (%s), use legacy vertex input\n");
    }

    void ReportMirrorDescriptorFallback(const char *material_name,
                                        const char *phase,
                                        const char *reason)
    {
        ReportMirrorFallbackWithPhaseReason(
            material_name,
            phase,
            reason,
            kShaderGenDescriptorFallbackPhaseBuildFailed,
            "[RendererShaderGenAdapter] material=%s mirror descriptor %s (%s), fallback to legacy descriptor layout\n");
    }
}
