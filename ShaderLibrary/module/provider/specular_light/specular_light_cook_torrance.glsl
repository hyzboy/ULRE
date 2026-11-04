// @ulre begin
// @ulre name specular_light_cook_torrance
// @ulre kind Utility
// @ulre priority 0
// @ulre slot specular_light_provider
// @ulre provides_capability direct_specular
// @ulre uses lighting_interface
// @ulre end
// 高光槽 — Cook-Torrance / GGX
//
// 只返回**未调制**的高光辐射项（不含 mainLightColor / shadowFactor），
// 由 LightingModel 槽统一调制。与 diffuse_* 槽完全解耦：
// 换漫反射模型不会改变本项，反之亦然。
#ifndef SPECULAR_COOK_TORRANCE_GLSL
#define SPECULAR_COOK_TORRANCE_GLSL

#include "module/interface/lighting_interface.glsl"

vec3 EvalSpecularLight(LightingInput lighting)
{
    const vec3 N = normalize(lighting.normal);
    const vec3 V = lighting.viewDir;
    const vec3 L = lighting.mainLightDir;

    const float NdotL = max(dot(N, L), 0.0);
    const float NdotV = max(dot(N, V), 1e-4);
    const vec3  H     = normalize(V + L);
    const float NdotH = max(dot(N, H), 0.0);
    const float VdotH = max(dot(V, H), 0.0);

    // roughness 语义 = perceptualRoughness；alpha = perceptual^2（Filament 同名变量）
    const float perceptualRoughness = lighting.roughness;
    const float alpha = perceptualRoughness * perceptualRoughness;

    // F0：metallic 工作流。fresnel 字段语义 = 电介质 F0（默认 0.04，
    // 即 Filament computeDielectricF0(reflectance=0.5) 的结果），
    // f0 = baseColor*metallic + fresnel*(1-metallic)，与 Filament computeF0 等价。
    const vec3 F0 = mix(vec3(lighting.fresnel), lighting.baseColor, lighting.metallic);

    // NDF —— D_GGX（拉格朗日恒等式稳健版，Filament surface_brdf.fs）
    const vec3 NxH = cross(N, H);
    const float D = ULRE_LIT_D_GGX_Correlated(NdotH, alpha, NxH);

    // 可见项 —— V_SmithGGXCorrelated（height-correlated），已含 1/(4 NoV NoL)
    const float Vis = ULRE_LIT_V_SmithGGXCorrelated(alpha, NdotV, NdotL);

    // Fresnel —— F_Schlick + F90（Disney：f90 = clamp(50*reflectance*f0,0,1)，
    // reflectance = sqrt(f0/0.16)；f0=0.04 时恰为 1，与 glTF/Filament 默认一致）
    const float reflectance = sqrt(lighting.fresnel * 6.25);
    const float F90 = clamp(50.0 * reflectance * lighting.fresnel, 0.0, 1.0);
    const vec3  F   = ULRE_LIT_F_Schlick_F90(F0, F90, VdotH);

    // spec = D * V * F * NoL（V 即 G/(4 NoV NoL)，直接乘 NoL 完成余弦加权）
    return D * Vis * F * NdotL;
}

#endif // SPECULAR_COOK_TORRANCE_GLSL
