#pragma once

#include <hgl/mtl/ShaderCodeModule.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>
#include <hgl/type/ValueArray.h>
#include <vulkan/vulkan.h>
#include <string>

namespace hgl::graph
{
    class GeometryVertexFormat;
}

namespace hgl::graph::mtl
{
    // One selected provider edge: which required semantic the provider satisfies.
    struct ShaderCodeModuleProviderSelection
    {
        ShaderCodeModuleSemantic requirement = ShaderCodeModuleSemantic::Unknown;
        const ShaderCodeModuleDefinition *provider = nullptr;
    };

    // ValueArray<T> instantiates its virtual Find() for every concrete T, which
    // requires an equality operator.
    inline bool operator==(const ShaderCodeModuleProviderSelection &lhs,
                           const ShaderCodeModuleProviderSelection &rhs) noexcept
    {
        return lhs.requirement == rhs.requirement && lhs.provider == rhs.provider;
    }

    // A candidate provider rejected for a required semantic, with the reason.
    struct ShaderCodeModuleRejectDiagnostic
    {
        ShaderCodeModuleSemantic requirement = ShaderCodeModuleSemantic::Unknown;
        const ShaderCodeModuleDefinition *candidate = nullptr;
        const char *reason = nullptr;
    };

    inline bool operator==(const ShaderCodeModuleRejectDiagnostic &lhs,
                           const ShaderCodeModuleRejectDiagnostic &rhs) noexcept
    {
        return lhs.requirement == rhs.requirement && lhs.candidate == rhs.candidate;
    }

    struct ShaderCodeModuleResolutionResult
    {
        bool resolved = false;
        ValueArray<ShaderCodeModuleProviderSelection> selections;
        ValueArray<ShaderCodeModuleRejectDiagnostic> diagnostics;
    };

    /**
     * Hash the normalized selected provider graph for use in a stage key.
     * Selection order is canonicalized by requirement semantic and provider
     * identity, so equivalent graphs produce the same digest.
     */
    uint64 GetShaderCodeModuleProviderGraphHash(
        const ShaderCodeModuleResolutionResult &result) noexcept;

    /**
     * Compose selected provider source in resolver dependency order.
     * Providers selected for multiple semantics are emitted once.
     */
    bool ComposeShaderCodeModuleProviderGraph(
        const ShaderCodeModuleResolutionResult &result,
        std::string &out_glsl);
}
