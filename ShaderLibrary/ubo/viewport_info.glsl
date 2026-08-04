// @ulre begin
// @ulre name viewport_info
// @ulre kind Utility
// @ulre priority 0
// @ulre provide Viewport
// @ulre end
#ifndef HGL_UBO_VIEWPORT_INFO_GLSL
#define HGL_UBO_VIEWPORT_INFO_GLSL

#define SCENE_VIEWPORT_UBO \
    layout(set=SCENE_SET, binding=VIEWPORT_BINDING) uniform ViewportInfo \
    { \
        mat4 ortho_matrix; \
        uvec2 canvas_resolution; \
        uvec2 viewport_resolution; \
        vec2 inv_viewport_resolution; \
    } viewport

#endif
