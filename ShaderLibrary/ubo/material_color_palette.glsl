// @ulre begin
// @ulre name material_color_palette
// @ulre kind Utility
// @ulre priority 0
// @ulre end
#ifndef HGL_UBO_MATERIAL_COLOR_PALETTE_GLSL
#define HGL_UBO_MATERIAL_COLOR_PALETTE_GLSL

#define MATERIAL_COLOR_PALETTE_UBO \
    layout(scalar, set=MATERIAL_SET, binding=0) uniform ColorPalette \
    { \
        vec4 color[256]; \
    } color_palette

#endif
