#pragma once

#include <string>

namespace hgl::graph
{
    inline constexpr const char *kShaderGenStrictGateUnknownCategory = "StrictGate.Unknown";
    inline constexpr const char *kShaderGenStrictGatePrebuildCategory = "StrictGate.Prebuild";
    inline constexpr const char *kShaderGenStrictGateSpvCategory = "StrictGate.Spv";
    inline constexpr const char *kShaderGenStrictGateVertexCategory = "StrictGate.Vertex";
    inline constexpr const char *kShaderGenStrictGateDescriptorCategory = "StrictGate.Descriptor";
    inline constexpr const char *kShaderGenStrictGateProfileCategory = "StrictGate.Profile";

    inline constexpr const char *kShaderGenDescriptorFallbackPhaseLayoutMismatch = "layout mismatch";
    inline constexpr const char *kShaderGenDescriptorFallbackPhaseBuildFailed = "layout build failed";
    inline constexpr const char *kShaderGenMirrorPreferredAbortPrefix = "mirror-preferred build aborted: ";

    std::string BuildMirrorPreferredAbortReason(const char *reason);

    using ShaderGenContractGateLogSink = void(*)(const char *message);

    void SetShaderGenContractGateLogSink(ShaderGenContractGateLogSink sink);
    void ResetShaderGenContractGateLogSink();

    void ReportMirrorPreferredStrictAbort(const char *material_name,
                                          const char *category,
                                          const char *reason);

    void ReportMirrorSPVFallback(const char *material_name,
                                 const char *reason);

    void ReportMirrorVertexFallback(const char *material_name,
                                    const char *reason);

    void ReportMirrorDescriptorFallback(const char *material_name,
                                        const char *phase,
                                        const char *reason);
}
