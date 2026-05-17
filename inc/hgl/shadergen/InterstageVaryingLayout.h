#pragma once

#include <string>

namespace hgl::graph
{

/// Canonical enumeration of all VS→FS interstage varyings.
/// The assigned integer values are the authoritative GLSL layout(location=N) indices.
/// varying_vs.glsl and varying_fs.glsl must mirror these numbers exactly.
enum class InterstageVarying : int
{
    MaterialInstanceID = 0,  ///< flat uint  — always present
    WorldPos           = 1,  ///< vec3       — HAS_POSITION
    WorldNormal        = 2,  ///< vec3       — HAS_NORMAL
    UV0                = 3,  ///< vec2       — HAS_TEXCOORD
    VertexColor        = 4,  ///< vec4       — HAS_COLOR
    Direction          = 6,  ///< vec3       — HAS_DIRECTION
    Luminance          = 7,  ///< float      — HAS_LUMINANCE
    ClipPos            = 8,  ///< vec4       — HAS_CLIP_POS
    WorldTangent       = 9,  ///< vec4       — HAS_TANGENT

    COUNT
};

/// Interpolation qualifier for a varying.
enum class VaryingInterp : uint8_t
{
    Smooth  = 0,  ///< default (smooth)
    Flat    = 1,  ///< flat (no interpolation, e.g. integers)
    NoPerspective = 2,
};

/// Full descriptor for one interstage varying.
struct VaryingDesc
{
    InterstageVarying  varying;
    int                location;     ///< layout(location=N) — mirrors enum value
    const char        *glsl_type;    ///< GLSL type string, e.g. "vec3", "uint"
    const char        *name;         ///< varying variable name in GLSL
    const char        *guard_macro;  ///< HAS_* define that gates this varying (nullptr = always present)
    VaryingInterp      interp;
};

/// Returns the canonical descriptor for a varying.
/// Asserts/returns nullptr for out-of-range values.
const VaryingDesc *GetVaryingDesc(InterstageVarying v);

/// Emit a single VS output declaration line, e.g.:
///   "layout(location=0) flat out uint fragMaterialInstanceID;\n"
/// If guard_macro is set, wraps in #ifdef / #endif.
std::string EmitVSOutput(InterstageVarying v);

/// Emit a single FS input declaration line, e.g.:
///   "layout(location=0) flat in uint fragMaterialInstanceID;\n"
/// If guard_macro is set, wraps in #ifdef / #endif.
std::string EmitFSInput(InterstageVarying v);

/// Emit ALL VS output declarations (respects guard macros).
std::string EmitAllVSOutputs();

/// Emit ALL FS input declarations (respects guard macros).
std::string EmitAllFSInputs();

} // namespace hgl::graph
