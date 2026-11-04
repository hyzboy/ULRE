// @ulre begin
// @ulre name specular_light_anisotropic
// @ulre kind Utility
// @ulre priority 0
// @ulre slot specular_light_provider
// @ulre provides_capability direct_specular
// @ulre uses lighting_interface
// @ulre end
// 高光槽 — 各向异性 GGX（对齐 Google Filament 各向异性 BRDF）
//
// 与 specular_light_cook_torrance 竞争**同一个槽位**：anisotropy = 0 时
// 数学上退化为各向同性 GGX；anisotropy > 0 时呈现拉丝/刷痕高光。
//
// 简化说明（阶段一）：
//   - 切线基暂由法线稳定推导（ULRE_LIT_StableTangent），不读取模型切线；
//     待高级数据行提供各向异性方向（tangent 旋转）后升级；
//   - 方向分布固定对齐推导切线，暂不支持 KHR 的 anisotropyRotation 纹理。
#ifndef SPECULAR_ANISOTROPIC_GLSL
#define SPECULAR_ANISOTROPIC_GLSL

#include "module/interface/lighting_interface.glsl"

vec3 EvalSpecularLight(LightingInput lighting)
{
    const vec3 N = normalize(lighting.normal);
    const vec3 V = lighting.viewDir;
    const vec3 L = lighting.mainLightDir;

    const float NoL = max(dot(N, L), 0.0);
    if (NoL <= 0.0)
    {
        return vec3(0.0);
    }
    const float NoV = max(dot(N, V), 1e-4);
    const vec3  H   = normalize(V + L);
    const float NoH = max(dot(N, H), 0.0);
    const float VoH = max(dot(V, H), 0.0);

    // Filament: aspect = sqrt(1 - anisotropy * (1 - 0.02))，
    // 切向/横向 roughness 沿 aspect 伸缩并夹紧，防止 alpha 退化为 0。
    const float alpha  = lighting.roughness * lighting.roughness;
    const float aspect = sqrt(clamp(1.0 - lighting.anisotropy * 0.98, 0.0, 1.0));
    const float at = clamp(alpha / aspect, 0.002025, 1.0); // 0.045^2
    const float ab = clamp(alpha * aspect, 0.002025, 1.0);

    // 切线基（稳定推导，见文件头简化说明）
    const vec3 T = ULRE_LIT_StableTangent(N);
    const vec3 B = cross(N, T);

    const float ToV = dot(T, V);
    const float BoV = dot(B, V);
    const float ToH = dot(T, H);
    const float BoH = dot(B, H);

    // NDF —— D_GGX_Anisotropic（归一化）
    const float a2 = at * ab;
    const vec3  vv = vec3(ab * ToH, at * BoH, a2 * NoH);
    const float v2 = max(dot(vv, vv), 1e-14);
    const float w2 = a2 / v2;
    const float D  = a2 * w2 * w2 * (1.0 / ULRE_PI);

    // Vis —— V_SmithGGXCorrelated（height-correlated，各向异性推广）
    const float lambdaV = NoL * sqrt(at * at * ToV * ToV + ab * ab * BoV * BoV + NoV * NoV);
    const float lambdaL = NoV * sqrt(at * at * ToH * ToH + ab * ab * BoH * BoH + NoL * NoL);
    const float Vis = 0.5 / max(lambdaV + lambdaL, 1e-7);

    // Fresnel —— 与 cook_torrance 相同的 F0/F90 约定
    const vec3  F0          = mix(vec3(lighting.fresnel), lighting.baseColor, lighting.metallic);
    const float reflectance = sqrt(lighting.fresnel * 6.25);
    const float F90           = clamp(50.0 * reflectance * lighting.fresnel, 0.0, 1.0);
    const vec3  F             = ULRE_LIT_F_Schlick_F90(F0, F90, VoH);

    return D * Vis * F * NoL;
}

#endif // SPECULAR_ANISOTROPIC_GLSL
