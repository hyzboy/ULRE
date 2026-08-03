#ifndef HGL_UBO_MATERIAL_COLOR_PALETTE_GLSL
#define HGL_UBO_MATERIAL_COLOR_PALETTE_GLSL

#define MATERIAL_COLOR_PALETTE_UBO \
    layout(scalar, set=MATERIAL_SET, binding=0) uniform ColorPattle \
    { \
        vec4 color[256]; \
    } color_pattle

#endif
