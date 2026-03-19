#pragma once

#include <hgl/shadergen/ShaderLayoutContract.h>
#include <string>

namespace hgl::graph
{
    /**
     * Emits a block of GLSL #define lines from a ShaderLayoutContract.
     *
     * Output structure:
     *   // ---- Auto-generated layout defines ----
     *   // Vertex input locations
     *   #define POSITION_LOCATION 0
     *   ...
     *   // Descriptor sets
     *   #define SCENE_SET 0
     *   ...
     *   // Descriptor bindings
     *   #define VIEWPORT_BINDING 0
     *   ...
     *   // ----------------------------------------
     *   (blank line)
     *
     * Sections that have no entries are omitted.
     * Returns an empty string if the contract is empty.
     *
     * The output is intended to be prepended to shader source so that the
     * #define names shadow any fallback values in descriptor_macros.glsl.
     */
    std::string EmitShaderLayoutDefines(const ShaderLayoutContract &contract);

    /**
     * Returns a human-readable dump of the contract for debug logging.
     * Format: one "  MACRO_NAME = N" per line, with section headers.
     */
    std::string DumpShaderLayoutContract(const ShaderLayoutContract &contract);

}  // namespace hgl::graph
