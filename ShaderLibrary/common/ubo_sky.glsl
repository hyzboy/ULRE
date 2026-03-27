#ifndef UBO_SKY_GLSL
#define UBO_SKY_GLSL

// @require UBO(SkyInfo)

layout(set=STATIC_SET, binding=SKY_BINDING) uniform SkyInfo
{
    vec4 base_sky_color;
    vec4 sun_direction;
    vec4 sun_color;
    vec4 halo_color;
    vec4 moon_color;
    float sun_ang_deg;
    float sun_intensity;
    float moon_intensity;
    float halo_intensity;
} sky;

#endif