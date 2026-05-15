#ifndef UBO_COLOR_PALETTE_GLSL
#define UBO_COLOR_PALETTE_GLSL

#ifndef PERMATERIAL_SET
#define PERMATERIAL_SET 0
#endif
#ifndef COLOR_PALETTE_BINDING
#define COLOR_PALETTE_BINDING 0
#endif

layout(scalar, set=PERMATERIAL_SET, binding=COLOR_PALETTE_BINDING) uniform ColorPalette
{
    uint color[256];
} color_palette;

#endif
