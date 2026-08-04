// @ulre begin
// @ulre name sky_info
// @ulre kind Utility
// @ulre priority 0
// @ulre provide SkyLight
// @ulre end
#ifndef HGL_UBO_SKY_INFO_GLSL
#define HGL_UBO_SKY_INFO_GLSL

#define SCENE_SKY_UBO \
    layout(set=SCENE_SET, binding=SKY_BINDING) uniform SkyInfo \
    { \
        vec4 base_sky_color; \
        vec4 sun_direction; \
        vec4 sun_color; \
        vec4 halo_color; \
        vec4 moon_color; \
        float sun_ang_deg; \
        float sun_intensity; \
        float moon_intensity; \
        float halo_intensity; \
    } sky

#endif
