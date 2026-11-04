// @ulre begin
// @ulre name subsurface_light_wrap
// @ulre kind Utility
// @ulre priority 0
// @ulre slot subsurface_light_provider
// @ulre provides_capability direct_subsurface
// @ulre uses lighting_interface
// @ulre end
// 次表面槽 — 简易版（wrap 漫反射增量 + 薄壳背散射）
//
// 只返回**叠加在漫反射槽之上**的未调制增量项（不含 mainLightColor /
// shadowFactor），因此返回的是 (wrapDiffuse - lambertDiffuse) 的差值，
// 不会与 diffuse_light_lambert 重复计费。
//
// 模型（刻意保持简单，非 Filament 的 screen-space 多散射 SSSSS）：
//   wrap  —— "Gastal & Johnson 2012" 加权光照余弦 (N·L + w)/(1 + w)，
//            w = subsurfacePower；
//   背散射 —— "Thin-shell diffusion"（KHR_materials_transmission 风格）：
//            dot(V, -L) 的幂 * thickness * albedo，模拟光穿透薄物体。
#ifndef SUBSURFACE_WRAP_GLSL
#define SUBSURFACE_WRAP_GLSL

#include "module/interface/lighting_interface.glsl"

vec3 EvalSubsurfaceLight(LightingInput lighting)
{
    if (lighting.subsurfacePower <= 0.0 && lighting.thickness <= 0.0)
    {
        return vec3(0.0);
    }

    const vec3  N   = normalize(lighting.normal);
    const vec3  V   = lighting.viewDir;
    const vec3  L   = lighting.mainLightDir;
    const float NdL = dot(N, L);

    const vec3 albedo = lighting.baseColor * (1.0 - lighting.metallic);
    const float w = lighting.subsurfacePower;

    // wrap 漫反射增量：max(NdL + w, 0)/(1+w) - max(NdL, 0)
    const float wrapDiffuse    = max(NdL + w, 0.0) / (1.0 + w);
    const float lambertDiffuse = max(NdL, 0.0);
    vec3 term = albedo * (wrapDiffuse - lambertDiffuse) * w / ULRE_PI;

    // 薄壳背散射：光从背面穿过薄片进入视线
    const float back = pow(clamp(dot(V, -L), 0.0, 1.0), 3.0);
    term += albedo * back * lighting.thickness * 0.5;

    return term;
}

#endif // SUBSURFACE_WRAP_GLSL
