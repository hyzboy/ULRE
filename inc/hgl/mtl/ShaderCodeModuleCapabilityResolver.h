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
    // A capability contributed by a single Geometry vertex attribute.
    struct ShaderCodeModuleGeometryCapability
    {
        ShaderCodeModuleSemantic semantic = ShaderCodeModuleSemantic::Unknown;
        uint32 numeric_class_mask = 0;
        uint8 component_count = 0;
    };

    inline bool operator==(const ShaderCodeModuleGeometryCapability &lhs,
                           const ShaderCodeModuleGeometryCapability &rhs) noexcept
    {
        return lhs.semantic == rhs.semantic
            && lhs.numeric_class_mask == rhs.numeric_class_mask
            && lhs.component_count == rhs.component_count;
    }

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

    struct ShaderCodeModuleResolutionRequest
    {
        // Surface requirements to satisfy (ProducedSemantic drives provider
        // selection; GeometryAttribute/Resource are validated directly).
        const ShaderCodeModuleSemanticRequirement *requirements = nullptr;
        uint32 requirement_count = 0;

        const ShaderCodeModuleGeometryCapability *geometry_capabilities = nullptr;
        uint32 geometry_capability_count = 0;

        const ShaderCodeModuleSemantic *resources = nullptr;
        uint32 resource_count = 0;

    };

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
