// @ulre begin
// @ulre name sky_atmosphere
// @ulre kind Utility
// @ulre priority 0
// @ulre slot ambient_light_provider
// @ulre provides_capability ambient_diffuse
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

vec3 EvalSkyAtmosphere(vec3 direction)
{
    vec3 dir = normalize(direction);
    vec3 sun_dir = GetSkyMainLightDir();
    float elevation = clamp(dir.z * 0.5 + 0.5, 0.0, 1.0);
    float horizon = 1.0 - clamp(dir.z, 0.0, 1.0);
    float sun_cos = clamp(dot(dir, sun_dir), -1.0, 1.0);

    // Compact Rayleigh/Mie approximation using the existing SkyInfo inputs.
    vec3 rayleigh = vec3(0.24, 0.48, 1.0)
                  * pow(max(elevation, 1e-3), 0.35);
    float mie_phase = (1.0 - 0.76 * 0.76)
                    / max(1e-3, 4.0 * 3.14159265
                        * pow(1.0 + 0.76 * 0.76 - 2.0 * 0.76 * sun_cos, 1.5));
    vec3 mie = sky.halo_color.rgb * mie_phase * sky.halo_intensity;
    vec3 horizon_tint = mix(
        sky.base_sky_color.rgb,
        vec3(1.0, 0.42, 0.08) * 1.2,
        clamp(horizon, 0.0, 1.0));

    return max(
        horizon_tint * (0.35 + 0.65 * rayleigh)
        + mie * (0.5 + 0.5 * elevation),
        vec3(0.0));
}

vec3 GetSkyAmbientColor()
{
    return EvalSkyAtmosphere(vec3(0.0, 0.0, 1.0));
}

#endif // SKY_ATMOSPHERE_GLSL
