// @ulre begin
// @ulre name sheen_light_charlie
// @ulre kind Utility
// @ulre priority 0
// @ulre slot sheen_light_provider
// @ulre provides_capability direct_sheen
// @ulre uses lighting_interface
// @ulre end
// 绒面槽 — Charlie NDF + Neubelt Vis（对齐 Google Filament shading_sheen / surface_shading_model_standard.fs）
//
// 只返回**未调制**的绒面辐射项（不含 mainLightColor / shadowFactor，但含 NoL 投影）。
// 对基底漫反射的能量损失衰减由 LightingModel 处理。
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

    // Vis —— Neubelt（Filament visibilityCloth 同款）
    const float Vis = ULRE_LIT_V_Neubelt(NoV, NoL);

    // Filament surface_shading_model_standard.fs:
    //   sheenLobe = (D * V) * pixel.sheenColor
    //   surfaceShading = ... * NoL
    return (D * Vis * NoL) * lighting.sheenColor;
}

#endif // SHEEN_CHARLIE_GLSL
