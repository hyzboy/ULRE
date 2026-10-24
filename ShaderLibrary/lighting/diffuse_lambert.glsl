// @ulre begin
// @ulre name diffuse_lambert
// @ulre kind Utility
// @ulre priority 0
// @ulre slot diffuse_light_provider
// @ulre provides_capability direct_diffuse
// @ulre uses lighting_interface
// @ulre end
// 漫反射槽 — Lambert（能量守恒的 F-kd 加权版本）
//
// 只返回**未调制**的漫反射辐射项：不含 mainLightColor、不含 shadowFactor。
// 调制由 LightingModel 槽统一完成，故本模块与高光槽可以任意组合，
// 且换成 specular_none 时漫反射结果完全不受影响。
#ifndef DIFFUSE_LAMBERT_GLSL
#define DIFFUSE_LAMBERT_GLSL

#include "common/lighting_interface.glsl"

vec3 EvalDiffuseLight(LightingInput lighting)
{
    const vec3 N = normalize(lighting.normal);
    const vec3 V = lighting.viewDir;
    const vec3 L = lighting.mainLightDir;

    const float NdotL = max(dot(N, L), 0.0);
    const vec3  H     = normalize(V + L);
    const float VdotH = max(dot(V, H), 0.0);

    const vec3 F0 = mix(vec3(lighting.fresnel), lighting.baseColor, lighting.metallic);
    const vec3 F  = ULRE_LIT_F_Schlick(VdotH, F0);
    const vec3 kd = (1.0 - F) * (1.0 - lighting.metallic);

    return kd * lighting.baseColor / 3.14159265 * NdotL;
}

#endif // DIFFUSE_LAMBERT_GLSL
