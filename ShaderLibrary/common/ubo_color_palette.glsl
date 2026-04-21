#ifndef UBO_COLOR_PALETTE_GLSL
#define UBO_COLOR_PALETTE_GLSL

layout(scalar, set=PERMATERIAL_SET, binding=COLOR_PALETTE_BINDING) uniform ColorPalette
{
    uint color[256];
} color_palette;

#endif