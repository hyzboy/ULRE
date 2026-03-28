#ifndef SKYLIGHT_IBL_GLSL
#define SKYLIGHT_IBL_GLSL

#define ULRE_SKY_SUN_DIR    normalize(sky.sun_direction.xyz)
#define ULRE_SKY_SUN_COLOR  (sky.sun_color.rgb * sky.sun_intensity)
#define ULRE_SKY_BASE_COLOR (sky.base_sky_color.rgb)

vec3 ULRE_GetSkyLightDir()
{
    return ULRE_SKY_SUN_DIR;
}

vec3 ULRE_GetSkyLightColor()
{
    return ULRE_SKY_SUN_COLOR;
}

vec3 ULRE_GetSkyAmbientColor()
{
    // stub: actual IBL CubeMap sample (textureLod(env_ibl, N, roughness*mip)) to be implemented
    return ULRE_SKY_BASE_COLOR;
}

#endif
