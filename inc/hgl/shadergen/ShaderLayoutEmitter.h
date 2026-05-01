#pragma once

#include <hgl/shadergen/ShaderLayoutContract.h>
#include <hgl/common/PositionProvider.h>
#include <iosfwd>
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
    *   #define STATIC_SET 0
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
    * The output is intended to be prepended to shader source and acts as the
    * authoritative source of GLSL layout macros.
     */
    std::string EmitShaderLayoutDefines(const ShaderLayoutContract &contract);

    /**
     * Returns a human-readable dump of the contract for debug logging.
     * Format: one "  MACRO_NAME = N" per line, with section headers.
     */
    std::string DumpShaderLayoutContract(const ShaderLayoutContract &contract);

    /**
     * Emit position-input declarations for the given PositionProvider into `out`.
     *
     * - DirectVec3: inlines `layout(location=N) in vec3 inPosition;` + `#define GetPositionLocal()`
     * - Others with vab_count > 0: emits `#define POSITION_LOCATION N` then `#include "<glsl_path>"`
     * - Others without VAB: emits `#include "<glsl_path>"` only
     */
    void EmitPositionInput(std::ostream       &out,
                           const PositionProvider &p,
                           int                 position_location);

}  // namespace hgl::graph
