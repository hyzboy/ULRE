/// get_position_2d.glsl - 2D position fetch utility
///
/// The following #defines must be set before including this file
/// (typically injected by Build2DVertexPreamble):
///   COORD_ORTHO         - orthographic projection via ubo_viewport
///   COORD_ZERO_TO_ONE   - remap [0,1] input coords to [-1,1] NDC
///   HAS_LOCAL_TO_WORLD  - apply LocalToWorld transform from ssbo_transform
///   POSITION_TYPE       - input attribute type (vec2 / ivec2 / ...)
///   POSITION_LOCATION   - input attribute location (set by layout defines)

#ifdef COORD_ORTHO
#include "common/ubo_viewport.glsl"
#endif

#ifdef HAS_LOCAL_TO_WORLD
#include "common/ssbo_transform.glsl"
#endif

layout(location=POSITION_LOCATION) in POSITION_TYPE Position;

vec4 GetPosition2D()
{
#if defined(COORD_ORTHO) && defined(HAS_LOCAL_TO_WORLD)
    return GetTransform() * viewport.ortho_matrix * vec4(vec2(Position), 0, 1);
#elif defined(COORD_ORTHO)
    return viewport.ortho_matrix * vec4(vec2(Position), 0, 1);
#elif defined(COORD_ZERO_TO_ONE) && defined(HAS_LOCAL_TO_WORLD)
    return GetTransform() * vec4(vec2(Position) * 2.0 - 1.0, 0, 1);
#elif defined(COORD_ZERO_TO_ONE)
    return vec4(vec2(Position) * 2.0 - 1.0, 0, 1);
#elif defined(HAS_LOCAL_TO_WORLD)
    return GetTransform() * vec4(vec2(Position), 0, 1);
#else
    return vec4(vec2(Position), 0, 1);
#endif
}
