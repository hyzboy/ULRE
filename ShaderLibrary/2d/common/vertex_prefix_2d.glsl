
#ifndef VERTEX_PREFIX_2D_GLSL
#define VERTEX_PREFIX_2D_GLSL

#ifdef COORD_ORTHO
#include "common/scene_ubo.glsl"
SCENE_VIEWPORT_UBO;
#endif

#ifdef HAS_L2W
#include "common/l2w_ssbo.glsl"
#endif

layout(location=POSITION_LOCATION) in POSITION_FORMAT Position;

vec4 GetPosition2D()
{
#if defined(COORD_ORTHO) && defined(HAS_L2W)
    return GetTransform() * viewport.ortho_matrix * vec4(vec2(Position), 0, 1);
#elif defined(COORD_ORTHO)
    return viewport.ortho_matrix * vec4(vec2(Position), 0, 1);
#elif defined(COORD_ZEROTOONE) && defined(HAS_L2W)
    return GetTransform() * vec4(vec2(Position) * 2.0 - 1.0, 0, 1);
#elif defined(COORD_ZEROTOONE)
    return vec4(vec2(Position) * 2.0 - 1.0, 0, 1);
#elif defined(HAS_L2W)
    return GetTransform() * vec4(vec2(Position), 0, 1);
#else
    return vec4(vec2(Position), 0, 1);
#endif
}

#endif 