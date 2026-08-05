// @ulre begin
// @ulre name sky_simple_color
// @ulre kind Utility
// @ulre priority 0
// @ulre end
// Sky Simple Color — 静态简单天光与方向光
#ifndef SKY_SIMPLE_COLOR_GLSL
#define SKY_SIMPLE_COLOR_GLSL

vec3 GetSkyMainLightDir()
{
    return normalize(vec3(0.577, 0.577, 0.577));
}

vec3 GetSkyMainLightColor()
{
    return vec3(1.0, 0.95, 0.85);
}

vec3 GetSkyAmbientColor()
{
    return vec3(0.15, 0.18, 0.22);
}

#endif // SKY_SIMPLE_COLOR_GLSL
