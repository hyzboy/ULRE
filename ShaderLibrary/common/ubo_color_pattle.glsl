#ifndef UBO_COLOR_PATTLE_GLSL
#define UBO_COLOR_PATTLE_GLSL

layout(scalar, set=PERMATERIAL_SET, binding=COLOR_PATTLE_BINDING) uniform ColorPattle
{
    vec4 color[256];
} color_pattle;

#endif