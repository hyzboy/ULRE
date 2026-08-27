#pragma once

#include <hgl/mtl/GLSLCodeModule.h>
#include <hgl/mtl/GLSLCodeModuleRegistry.h>
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
    struct GLSLCodeModuleGeometryCapability
    {
        GLSLCodeModuleSemantic semantic = GLSLCodeModuleSemantic::Unknown;
        uint32 numeric_class_mask = 0;
        uint8 component_count = 0;
    };

    inline bool operator==(const GLSLCodeModuleGeometryCapability &lhs,
                           const GLSLCodeModuleGeometryCapability &rhs) noexcept
    {
        return lhs.semantic == rhs.semantic
            && lhs.numeric_class_mask == rhs.numeric_class_mask
            && lhs.component_count == rhs.component_count;
    }

    // One selected provider edge: which required semantic the provider satisfies.
    struct GLSLCodeModuleProviderSelection
    {
        GLSLCodeModuleSemantic requirement = GLSLCodeModuleSemantic::Unknown;
        const GLSLCodeModuleDefinition *provider = nullptr;
    };

    // ValueArray<T> instantiates its virtual Find() for every concrete T, which
    // requires an equality operator.
    inline bool operator==(const GLSLCodeModuleProviderSelection &lhs,
                           const GLSLCodeModuleProviderSelection &rhs) noexcept
    {
        return lhs.requirement == rhs.requirement && lhs.provider == rhs.provider;
    }

    // A candidate provider rejected for a required semantic, with the reason.
    struct GLSLCodeModuleRejectDiagnostic
    {
        GLSLCodeModuleSemantic requirement = GLSLCodeModuleSemantic::Unknown;
        const GLSLCodeModuleDefinition *candidate = nullptr;
        const char *reason = nullptr;
    };

    inline bool operator==(const GLSLCodeModuleRejectDiagnostic &lhs,
                           const GLSLCodeModuleRejectDiagnostic &rhs) noexcept
    {
        return lhs.requirement == rhs.requirement && lhs.candidate == rhs.candidate;
    }

    struct GLSLCodeModuleResolutionRequest
    {
        // Surface requirements to satisfy (ProducedSemantic drives provider
        // selection; GeometryAttribute/Resource are validated directly).
        const GLSLCodeModuleSemanticRequirement *requirements = nullptr;
        uint32 requirement_count = 0;

        const GLSLCodeModuleGeometryCapability *geometry_capabilities = nullptr;
        uint32 geometry_capability_count = 0;

        const GLSLCodeModuleSemantic *resources = nullptr;
        uint32 resource_count = 0;

    };

    struct GLSLCodeModuleResolutionResult
    {
        bool resolved = false;
        ValueArray<GLSLCodeModuleProviderSelection> selections;
        ValueArray<GLSLCodeModuleRejectDiagnostic> diagnostics;
    };

    /**
     * Hash the normalized selected provider graph for use in a stage key.
     * Selection order is canonicalized by requirement semantic and provider
     * identity, so equivalent graphs produce the same digest.
     */
    uint64 GetGLSLCodeModuleProviderGraphHash(
        const GLSLCodeModuleResolutionResult &result) noexcept;

    /**
     * Compose selected provider source in resolver dependency order.
     * Providers selected for multiple semantics are emitted once.
     */
    bool ComposeGLSLCodeModuleProviderGraph(
        const GLSLCodeModuleResolutionResult &result,
        std::string &out_glsl);
}
