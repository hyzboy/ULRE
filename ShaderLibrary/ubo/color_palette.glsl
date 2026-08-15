// @ulre begin
// @ulre name color_palette
// @ulre kind Utility
// @ulre priority 0
// @ulre end
#ifndef HGL_UBO_COLOR_PALETTE_GLSL
#define HGL_UBO_COLOR_PALETTE_GLSL

#define SCENE_COLOR_PALETTE_UBO \
    layout(scalar, set=SCENE_SET, binding=COLOR_PALETTE_BINDING) uniform ColorPalette \
    { \
        uint color[256]; \
    } color_palette

#endif
