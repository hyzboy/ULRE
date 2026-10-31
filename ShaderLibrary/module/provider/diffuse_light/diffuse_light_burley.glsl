// @ulre begin
// @ulre name diffuse_light_burley
// @ulre kind Utility
// @ulre priority 0
// @ulre slot diffuse_light_provider
// @ulre provides_capability direct_diffuse
// @ulre uses lighting_interface
// @ulre end
// 漫反射槽 — Burley（Disney 2012 讨论式漫反射）
//
// 在 Lambert 基础上加一个与视角/粗糙度相关的 retro-reflection 标量 fl，
// 让掠射角的布料/皮肤/灰泥看起来更“包裹”。签名与 diffuse_lambert 完全一致，
// 因此可以在不改动任何调用点的情况下整条替换。
#ifndef DIFFUSE_BURLEY_GLSL
#define DIFFUSE_BURLEY_GLSL

#include "module/interface/lighting_interface.glsl"

vec3 EvalDiffuseLight(LightingInput lighting)
{
    const vec3 N = normalize(lighting.normal);
    const vec3 V = lighting.viewDir;
    const vec3 L = lighting.mainLightDir;

    const float NdotL = max(dot(N, L), 0.0);
    const float NdotV = max(dot(N, V), 1e-4);
    const vec3  H     = normalize(V + L);
    const float VdotH = max(dot(V, H), 0.0);

    const float fd90 = 0.5 + 2.0 * lighting.roughness * VdotH * VdotH;
    const float fl = (1.0 + (fd90 - 1.0) * pow(1.0 - NdotL, 5.0))
                   * (1.0 + (fd90 - 1.0) * pow(1.0 - NdotV, 5.0));

    const vec3 F0 = mix(vec3(lighting.fresnel), lighting.baseColor, lighting.metallic);
    const vec3 F  = ULRE_LIT_F_Schlick(VdotH, F0);
    const vec3 kd = (1.0 - F) * (1.0 - lighting.metallic);

    return kd * lighting.baseColor / 3.14159265 * NdotL * fl;
}

#endif // DIFFUSE_BURLEY_GLSL
