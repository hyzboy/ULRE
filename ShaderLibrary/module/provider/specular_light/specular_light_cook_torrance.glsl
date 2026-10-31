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

    const float alpha2 = lighting.roughness * lighting.roughness
                       * lighting.roughness * lighting.roughness;
    const float D = ULRE_LIT_D_GGX(NdotH, alpha2);
    const float G = ULRE_LIT_G_Smith(NdotV, NdotL, lighting.roughness);
    const vec3  F0 = mix(vec3(lighting.fresnel), lighting.baseColor, lighting.metallic);
    const vec3  F  = ULRE_LIT_F_Schlick(VdotH, F0);

    return D * G * F / max(4.0 * NdotV * NdotL, 1e-4) * NdotL;
}

#endif // SPECULAR_COOK_TORRANCE_GLSL
