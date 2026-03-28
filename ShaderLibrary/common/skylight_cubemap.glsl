#ifndef SKYLIGHT_CUBEMAP_GLSL
#define SKYLIGHT_CUBEMAP_GLSL

#define ULRE_SKY_SUN_DIR    normalize(sky.sun_direction.xyz)
#define ULRE_SKY_SUN_COLOR  (sky.sun_color.rgb * sky.sun_intensity)
#define ULRE_SKY_BASE_COLOR (sky.base_sky_color.rgb)

uniform samplerCube SkyCubeMap;

vec3 ULRE_GetSkyLightDir()
{
    return ULRE_SKY_SUN_DIR;
}

vec3 ULRE_GetSkyLightColor()
{
    // TODO(stage-2): sun direction + CubeMap blend
    return ULRE_SKY_SUN_COLOR;
}

vec3 ULRE_GetSkyAmbientColor()
{
    // TODO(stage-2): CubeMap sample (expandable by normal/view/roughness)
    return ULRE_SKY_BASE_COLOR;
}

#endif
