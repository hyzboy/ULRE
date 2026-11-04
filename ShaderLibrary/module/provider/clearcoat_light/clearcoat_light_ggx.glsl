// @ulre begin
// @ulre name clearcoat_light_ggx
// @ulre kind Utility
// @ulre priority 0
// @ulre slot clearcoat_light_provider
// @ulre provides_capability direct_clearcoat
// @ulre uses lighting_interface
// @ulre end
// 清漆槽 — GGX 双层叶（算法与 Google Filament shading_clearcoat.fs 对齐）
//
// 只返回**未调制**的清漆辐射项（不含 mainLightColor / shadowFactor）。
// 基底能量衰减 (1 - clearCoatNoV) 由 LightingModel 槽统一处理，
// 因为衰减作用于 diffuse+specular，而本槽只能贡献加法项。
//
// 简化说明（相对 Filament）：
//   - 不使用独立的清漆法线贴图（clearCoatNormal 恒等于几何法线 N），
//     与 KHR_materials_clearcoat 未提供 normalTextureInfo 时一致；
//   - coat IOR 固定 1.5 ⇒ F0 = 0.04（glTF 约定）。
#ifndef CLEARCOAT_GGX_GLSL
#define CLEARCOAT_GGX_GLSL

#include "module/interface/lighting_interface.glsl"

vec3 EvalClearCoatLight(LightingInput lighting)
{
    if (lighting.clearCoat <= 0.0)
    {
        return vec3(0.0);
    }

    const vec3  N   = normalize(lighting.normal);
    const vec3  V   = lighting.viewDir;
    const vec3  L   = lighting.mainLightDir;
    const float NoL = max(dot(N, L), 0.0);
    if (NoL <= 0.0)
    {
        return vec3(0.0);
    }
    const float NoV = max(dot(N, V), 1e-4);
    const vec3  H   = normalize(V + L);
    const float NoH = max(dot(N, H), 0.0);
    const float VoH = max(dot(V, H), 0.0);

    // perceptualRoughness -> alpha（Filament: pow2(clearCoatRoughness)）
    const float alpha = lighting.clearCoatRoughness * lighting.clearCoatRoughness;

    // NDF —— D_GGX（ULRE_LIT_D_GGX 入参为 alpha2 = alpha^2）
    const float D = ULRE_LIT_D_GGX(NoH, alpha * alpha);

    // Vis —— V_Kelemen + SchlickGGX(a = 0.045)（Filament 清漆层固定值）
    const float Vis = ULRE_LIT_V_Kelemen(
        ULRE_LIT_SchlickGGX_S(NoV, 0.045),
        ULRE_LIT_SchlickGGX_S(NoL, 0.045));

    // Fresnel —— coat F0 = 0.04（IOR 1.5），f90 = 1
    const float F = ULRE_LIT_F_Schlick_S(0.04, 1.0, VoH);

    // 强度调制 —— F_Schlick(clearCoat, 0.0, NoV)
    const float attenuation = ULRE_LIT_F_Schlick_S(lighting.clearCoat, 0.0, NoV);

    return D * Vis * F * attenuation * NoL;
}

#endif // CLEARCOAT_GGX_GLSL
