#pragma once

// 统一 SkyLight 输入与可扩展天光模型选择。
// 约定：所有需要天光输入的材质都通过本块提供的函数读取。
//
// 可在材质侧通过 define 覆盖：
//   ULRE_SKYLIGHT_MODEL = ULRE_SKYLIGHT_MODEL_SIMPLE / IBL / ENVMAP / SH

#define ULRE_SKYLIGHT_GLSL_COMMON R"(
#define ULRE_SKYLIGHT_MODEL_SIMPLE  1
#define ULRE_SKYLIGHT_MODEL_IBL     2
#define ULRE_SKYLIGHT_MODEL_ENVMAP  3
#define ULRE_SKYLIGHT_MODEL_SH      4

#ifndef ULRE_SKYLIGHT_MODEL
    #define ULRE_SKYLIGHT_MODEL ULRE_SKYLIGHT_MODEL_SIMPLE
#endif

#define ULRE_SKY_SUN_DIR normalize(sky.sun_direction.xyz)
#define ULRE_SKY_SUN_COLOR (sky.sun_color.rgb * sky.sun_intensity)
#define ULRE_SKY_BASE_COLOR (sky.base_sky_color.rgb)

vec3 ULRE_GetSkyLightDir()
{
    return ULRE_SKY_SUN_DIR;
}

vec3 ULRE_GetSkyLightColor()
{
#if ULRE_SKYLIGHT_MODEL == ULRE_SKYLIGHT_MODEL_SH
    return max(ULRE_SKY_SUN_COLOR, ULRE_SKY_BASE_COLOR * 0.5);
#else
    return ULRE_SKY_SUN_COLOR;
#endif
}

vec3 ULRE_GetSkyAmbientColor()
{
#if ULRE_SKYLIGHT_MODEL == ULRE_SKYLIGHT_MODEL_IBL
    return ULRE_SKY_BASE_COLOR;
#elif ULRE_SKYLIGHT_MODEL == ULRE_SKYLIGHT_MODEL_ENVMAP
    return ULRE_SKY_BASE_COLOR;
#elif ULRE_SKYLIGHT_MODEL == ULRE_SKYLIGHT_MODEL_SH
    return ULRE_SKY_BASE_COLOR;
#else
    float h = clamp(ULRE_SKY_SUN_DIR.z * 0.5 + 0.5, 0.0, 1.0);
    vec3 grad = ULRE_SKY_BASE_COLOR * exp2(-(1.0 - h) * 0.8);

    float horizon = 1.0 - h;
    vec3 warm_tint = mix(vec3(1.0), vec3(1.0, 0.4, 0.05) * 1.2, clamp(horizon, 0.0, 1.0));
    vec3 scatter_mix = mix(grad, warm_tint, 0.5 * sky.sun_intensity);

    float atmosphere = sqrt(max(0.0, 1.0 - h));
    return mix(grad, scatter_mix, atmosphere * 0.7);
#endif
}
)"