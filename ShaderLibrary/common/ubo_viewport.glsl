#ifndef UBO_VIEWPORT_GLSL
#define UBO_VIEWPORT_GLSL

layout(set=STATIC_SET, binding=VIEWPORT_BINDING) uniform ViewportInfo
{
    mat4 ortho_matrix;
    uvec2 canvas_resolution;
    uvec2 viewport_resolution;
    vec2 inv_viewport_resolution;
} viewport;

#endif