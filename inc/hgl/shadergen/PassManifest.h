#pragma once

/// @file PassManifest.h
/// @brief Data-driven manifest types for the ShaderGen refactor.
///
/// These types form the declarative data model that replaces hand-written
/// builder functions. Step 2 of the refactor populates StageManifest tables
/// inside CompositorAssembler.cpp. Step 3 uses AttributeSemantic to drive
/// attribute-format decoding helpers.

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

} // namespace hgl::graph
