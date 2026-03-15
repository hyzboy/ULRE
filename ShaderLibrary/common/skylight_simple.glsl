// skylight_simple.glsl — Simple sky light model (GLSL include)
// Requires: sky UBO (SkyInfo) already declared via SCENE_SKY_UBO
//
// Provides:
//   vec3 ULRE_GetSkyLightDir()    — normalized sun direction
//   vec3 ULRE_GetSkyLightColor()  — sun color * intensity
//   vec3 ULRE_GetSkyAmbientColor()— gradient ambient from base_sky_color

#ifndef SKYLIGHT_SIMPLE_GLSL
#define SKYLIGHT_SIMPLE_GLSL

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
    float h = clamp(ULRE_SKY_SUN_DIR.z * 0.5 + 0.5, 0.0, 1.0);
    return ULRE_SKY_BASE_COLOR * exp2(-(1.0 - h) * 0.8);
}

#endif // SKYLIGHT_SIMPLE_GLSL
