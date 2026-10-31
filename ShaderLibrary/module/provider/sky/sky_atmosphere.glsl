// @ulre begin
// @ulre name sky_atmosphere
// @ulre kind Utility
// @ulre priority 0
// @ulre slot sky_provider
// @ulre uses scene_ubo
// @ulre uses descriptor_macros
// @ulre end
// Sky Atmosphere — 大气天光与太阳光数据源
#ifndef SKY_ATMOSPHERE_GLSL
#define SKY_ATMOSPHERE_GLSL

#include "contract/descriptor_macros.glsl"
#include "module/descriptor/ubo/scene_ubo.glsl"

// 主方向光数据自 **世界私有光源行**（`main_light`，行 0）来 —— T6b 起光的唯一真源。
vec3 GetMainLightDir()
{
    return normalize(main_light.direction_intensity.xyz);
}

vec3 GetMainLightColor()
{
    return max(main_light.color_angle.rgb * main_light.direction_intensity.w, vec3(0.05));
}

// 天空外观常量（原 `SkyInfo` 的 `base_sky_color` / `halo_intensity` 两个 authored 标量）。
// 2026-10-07 随「sky 段整删」搬成常量：这两个字段**全仓从来没有写者**（唯一调用点
// `example/Environment/AtmosphereSkyMinimal.cpp` 只调了一次 `EditSkyInfo()`、连返回值都没接、
// 未改任何字段）⇒ 取值即历史默认值 ⇒ 外观逐位不变（八台架/RTT/RTTCD/环境自检摘要均未变）。
// 将来做真正的天空方法（大气散射 / 天气）时按 R1 新建它**自己的**参数块，不要再往这里堆字段。
const vec3  kBaseSkyColor  = vec3(0.1, 0.3, 0.6);
const float kHaloIntensity = 0.1;

vec3 EvalSkyAtmosphere(vec3 direction)
{
    vec3 dir = normalize(direction);
    vec3 sun_dir = GetMainLightDir();
    float elevation = clamp(dir.z * 0.5 + 0.5, 0.0, 1.0);
    float horizon = 1.0 - clamp(dir.z, 0.0, 1.0);
    float sun_cos = clamp(dot(dir, sun_dir), -1.0, 1.0);

    // Compact Rayleigh/Mie 近似：形状由方向/仰角/相位推导，底色与光晕强度是上面的常量。
    vec3 rayleigh = vec3(0.24, 0.48, 1.0)
                  * pow(max(elevation, 1e-3), 0.35);
    float mie_phase = (1.0 - 0.76 * 0.76)
                    / max(1e-3, 4.0 * 3.14159265
                        * pow(1.0 + 0.76 * 0.76 - 2.0 * 0.76 * sun_cos, 1.5));
    // 光晕（Mie）色 = **主光色**：旧模型里它是主光色的同步副本 ⇒ 直接读光行，双源清零
    vec3 mie = main_light.color_angle.rgb * mie_phase * kHaloIntensity;
    vec3 horizon_tint = mix(
        kBaseSkyColor,
        vec3(1.0, 0.42, 0.08) * 1.2,
        clamp(horizon, 0.0, 1.0));

    return max(
        horizon_tint * (0.35 + 0.65 * rayleigh)
        + mie * (0.5 + 0.5 * elevation),
        vec3(0.0));
}

#endif // SKY_ATMOSPHERE_GLSL
