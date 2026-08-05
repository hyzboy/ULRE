// @ulre begin
// @ulre name sky_atmosphere
// @ulre kind Utility
// @ulre priority 0
// @ulre uses sky_info
// @ulre end
// Sky Atmosphere — 大气天光与太阳光数据源
#ifndef SKY_ATMOSPHERE_GLSL
#define SKY_ATMOSPHERE_GLSL

#include "common/descriptor_macros.glsl"
#include "ubo/sky_info.glsl"

vec3 GetSkyMainLightDir()
{
    return normalize(sky.sun_direction.xyz);
}

vec3 GetSkyMainLightColor()
{
    return max(sky.sun_color.rgb * sky.sun_intensity, vec3(0.05));
}

vec3 GetSkyAmbientColor()
{
    float h = clamp(normalize(sky.sun_direction.xyz).z * 0.5 + 0.5, 0.0, 1.0);
    return sky.base_sky_color.rgb * exp2(-(1.0 - h) * 0.8);
}

#endif // SKY_ATMOSPHERE_GLSL
