#pragma once

#include <hgl/graph/glsl/GLSLCodeModule.h>
#include <hgl/graph/glsl/GLSLCodeModuleRegistry.h>
#include <hgl/type/ValueArray.h>
#include <vulkan/vulkan.h>
#include <string>

namespace hgl::graph
{
    class GeometryVertexFormat;
}

namespace hgl::graph::mtl
{
    // Reserved provider flags. `Exclusive` marks a provider that must be the
    // only one covering its provided semantics; deterministic per-semantic
    // selection already guarantees this while the stage-graph policy lands.
    enum class GLSLCodeModuleProviderFlag : uint32
    {
        None = 0,
        Exclusive = 1u << 0
    };

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
        // selection; GeometryAttribute/Resource/Option are validated directly).
        const GLSLCodeModuleSemanticRequirement *requirements = nullptr;
        uint32 requirement_count = 0;

        const GLSLCodeModuleGeometryCapability *geometry_capabilities = nullptr;
        uint32 geometry_capability_count = 0;

        const GLSLCodeModuleSemantic *resources = nullptr;
        uint32 resource_count = 0;

        const GLSLCodeModuleSemantic *options = nullptr;
        uint32 option_count = 0;
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

    /**
     * Generic capability resolver for GLSL code modules.
     *
     * Converts concrete geometry formats into semantic/numeric-class
     * capabilities and selects provider modules purely from metadata:
     * surface ProducedSemantic requirements -> provider `provide` outputs,
     * provider requirements -> Geometry capabilities / resources / options /
     * other providers' outputs.
     *
     * Selection is deterministic: for each required semantic the candidates
     * are ordered by `priority` descending then module ID ascending, and the
     * first feasible candidate is chosen. Provider composition is supported;
     * produced dependencies are committed before their dependents so the
     * selection list is in dependency order.
     */
    class GLSLCodeModuleCapabilityResolver
    {
    public:
        /**
         * Map a concrete VkFormat to its numeric-class bitmask. Returns 0 for
         * formats that are not valid vertex input formats.
         */
        static uint32 GetNumericClassFromVkFormat(const VkFormat format);

        /**
         * Convert every attribute of a GeometryVertexFormat into a capability.
         * Unsupported semantics/formats are skipped.
         */
        static bool BuildGeometryCapabilities(const GeometryVertexFormat &format,
                                              ValueArray<GLSLCodeModuleGeometryCapability> &out);

        /**
         * Test a single geometry-attribute requirement against one capability.
         */
        static bool MatchGeometryCapability(const GLSLCodeModuleSemanticRequirement &requirement,
                                            const GLSLCodeModuleGeometryCapability &capability);

        /**
         * Resolve a surface requirement set against a registry and the inputs
         * of the current build request. On success `result.selections` holds
         * the deterministic provider chain (dependency order); `resolved` and
         * the return value are true only when every requirement is satisfied.
         */
        bool Resolve(const GLSLCodeModuleRegistry &registry,
                     const GLSLCodeModuleResolutionRequest &request,
                     GLSLCodeModuleResolutionResult &result) const;
    };
}
