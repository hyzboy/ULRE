#pragma once

#include <hgl/CoreType.h>
#include <hgl/shadergen/ShaderArtifactContract.h>

namespace hgl::graph::mtl
{
    enum class ShaderGenImplementationPath : uint8
    {
        Legacy = 0,
        Shadow,
        Contract
    };

    enum class ShaderGenDiagnosticEventKind : uint8
    {
        ShadowMatch = 0,
        ShadowMismatch,
        CacheHit,
        CacheMiss,
        CacheWrite,
        Fallback,
        ShadowModuleGraphBuilt,
        ShadowModuleGraphFailed
    };

    struct ShaderGenDiagnosticEvent
    {
        ShaderGenDiagnosticEventKind kind = ShaderGenDiagnosticEventKind::ShadowMatch;
        uint64 legacy_digest = 0;
        uint64 contract_digest = 0;
    };

    using ShaderGenDiagnosticCallback = void (*)(
        const ShaderGenDiagnosticEvent &event,
        void *user_data);

    struct ShaderGenMigrationOptions
    {
        ShaderGenImplementationPath implementation_path =
            ShaderGenImplementationPath::Legacy;
        ShaderArtifactCacheNamespace artifact_namespace =
            ShaderArtifactCacheNamespace::Legacy;
        ShaderGenDiagnosticCallback diagnostic_callback = nullptr;
        void *diagnostic_user_data = nullptr;
    };

    inline void ReportShaderGenDiagnostic(
        const ShaderGenMigrationOptions &options,
        const ShaderGenDiagnosticEvent &event)
    {
        if (options.diagnostic_callback)
            options.diagnostic_callback(event, options.diagnostic_user_data);
    }
}
