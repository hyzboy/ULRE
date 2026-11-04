// @ulre begin
// @ulre name sheen_light_charlie
// @ulre kind Utility
// @ulre priority 0
// @ulre slot sheen_light_provider
// @ulre provides_capability direct_sheen
// @ulre uses lighting_interface
// @ulre end
// 绒面槽 — Charlie NDF + Ashikhmin Vis（对齐 Google Filament shading_sheen.fs）
//
// 只返回**未调制**的绒面辐射项（不含 mainLightColor / shadowFactor）。
// 对基底漫反射的能量损失衰减 (1 - sheenEnergyLoss) 由 LightingModel 处理。
//
// 简化说明（相对 Filament 完整版）：
//   - 绒面只作用于直接光（Filament 同样如此，sheen 不进入 IBL specular）；
//   - 采用归一化 D_Charlie，能量补偿用 sheenEnergyLoss 多项式拟合。
#ifndef SHEEN_CHARLIE_GLSL
#define SHEEN_CHARLIE_GLSL

#include "module/interface/lighting_interface.glsl"

vec3 EvalSheenLight(LightingInput lighting)
{
    if (dot(lighting.sheenColor, lighting.sheenColor) <= 0.0)
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

    // NDF —— Charlie（Filament 直接传入 perceptualRoughness）
    const float D = ULRE_LIT_D_Charlie(NoH, lighting.sheenRoughness);

    // Vis —— Ashikhmin
    const float Vis = ULRE_LIT_V_Ashikhmin(NoV, NoL);

    // Fresnel —— F_Schlick(sheenColor, f90 = 1, VoH = NoH)
    const vec3 F = ULRE_LIT_F_Schlick_F90(lighting.sheenColor, 1.0, NoH);

    // 能量补偿：specular 自身乘以 (1 - energyLoss * NoL)
    const float alpha = lighting.sheenRoughness * lighting.sheenRoughness;
    const float energyLoss = ULRE_LIT_SheenEnergyLoss(NoV, NoL, alpha);

    return D * Vis * F * (1.0 - energyLoss * NoL) * NoL;
}

#endif // SHEEN_CHARLIE_GLSL
