#pragma once

/// @file PassManifest.h
/// @brief Data-driven manifest types for the ShaderGen refactor.
///
/// These types form the declarative data model that replaces hand-written
/// builder functions.  Step 2 of the refactor populates StageManifest tables
/// inside CompositorAssembler.cpp.  Steps 3-5 will use AttributeSemantic,
/// MaterialManifest, and FallbackRule to drive attribute-format selection and
/// degradation chains.

#include <hgl/common/VertexAttribDef.h>
#include <string>
#include <vector>

namespace hgl::graph
{
    // -----------------------------------------------------------------------
    // Attribute format registry (Step 3)
    // -----------------------------------------------------------------------

    /// A single wire-format encoding for a vertex attribute semantic.
    /// For example, Normal may be encoded as RGB32F (plain vec3) or as
    /// RG16F (octahedral), each with a different decode expression.
    struct AttributeEncoding
    {
        uint32_t    vk_format;       ///< VkFormat value for this encoding
        const char* input_glsl;      ///< GLSL input declaration fragment (e.g. "vec3")
        const char* decode_expr;     ///< Decode expression; '$' is replaced by the raw
                                     ///< input variable name at code-gen time.
                                     ///< Plain vec3: "$"; octahedral: "OctDecode($)"
    };

    /// Associates a logical VertexAttrib with the ordered list of supported
    /// wire-format encodings (best-quality first).
    struct AttributeSemantic
    {
        VertexAttrib                    attrib;        ///< Which VertexAttrib this semantic maps to
        const char*                     logical_type;  ///< GLSL type after decode (e.g. "vec3")
        std::vector<AttributeEncoding>  encodings;     ///< Ordered by preference (best first)
    };

    // -----------------------------------------------------------------------
    // Material degradation chain (Step 4)
    // -----------------------------------------------------------------------

    /// One entry in a material's attribute-supply degradation chain.
    /// When ALL listed attribs are absent from the mesh, the material switches
    /// to the specified surface variant.
    struct FallbackRule
    {
        std::vector<VertexAttrib> when_missing;   ///< Trigger: ALL of these attribs absent
        std::string               use_surface;    ///< Surface variant to switch to
        bool                      log_warning = true;
    };

    /// High-level description of a material variant: which surface function it
    /// uses by default, and the ordered fallback chain applied when required
    /// attributes are absent from the mesh.
    struct MaterialManifest
    {
        std::string               name;
        std::string               primary_surface;     ///< Default surface path (rel. ShaderLibrary/)
        std::vector<FallbackRule> fallback_chain;      ///< Tried in order; first match wins
        std::string               ultimate_fallback;   ///< Diagnostic material used when no rule matches
    };

} // namespace hgl::graph
