// @sfm:require UBO viewport
#ifndef UBO_VIEWPORT_GLSL
#define UBO_VIEWPORT_GLSL

#ifndef STATIC_SET
#define STATIC_SET 0
#endif
#ifndef VIEWPORT_BINDING
#define VIEWPORT_BINDING 0
#endif

layout(set=STATIC_SET, binding=VIEWPORT_BINDING) uniform ViewportInfo
{
    mat4 ortho_matrix;
    uvec2 canvas_resolution;
    uvec2 viewport_resolution;
    vec2 inv_viewport_resolution;
} viewport;

#endif
