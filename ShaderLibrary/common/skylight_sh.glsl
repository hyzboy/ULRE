// @sfm:require UBO sky
#ifndef SKYLIGHT_SH_GLSL
#define SKYLIGHT_SH_GLSL

#define ULRE_SKY_SUN_DIR    normalize(sky.sun_direction.xyz)
#define ULRE_SKY_SUN_COLOR  (sky.sun_color.rgb * sky.sun_intensity)
#define ULRE_SKY_BASE_COLOR (sky.base_sky_color.rgb)

vec3 ULRE_GetSkyLightDir()
{
    return ULRE_SKY_SUN_DIR;
}

vec3 ULRE_GetSkyLightColor()
{
    return max(ULRE_SKY_SUN_COLOR, ULRE_SKY_BASE_COLOR * 0.5);
}

vec3 ULRE_GetSkyAmbientColor()
{
    // stub: actual SH evaluation (sum Y_l_m * L_l_m) to be implemented
    return ULRE_SKY_BASE_COLOR;
}

#endif
