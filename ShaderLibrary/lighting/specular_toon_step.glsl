// @ulre begin
// @ulre name specular_toon_step
// @ulre kind Utility
// @ulre priority 0
// @ulre slot specular_light_provider
// @ulre provides_capability direct_specular
// @ulre uses lighting_interface
// @ulre end
// 高光槽 — 卡通硬高光块（NPR）
//
// 与 specular_cook_torrance 同签名：把连续 BRDF 换成 N·H 上的一个硬阶。
// 与 diffuse_toon 搭配即可得到完整的卡通光照，而主着色器一行不用改。
#ifndef SPECULAR_TOON_STEP_GLSL
#define SPECULAR_TOON_STEP_GLSL

#include "common/lighting_interface.glsl"

vec3 EvalSpecularLight(LightingInput lighting)
{
    const vec3 N = normalize(lighting.normal);
    const vec3 V = lighting.viewDir;
    const vec3 L = lighting.mainLightDir;

    const float NdotL = max(dot(N, L), 0.0);
    const vec3  H     = normalize(V + L);
    const float NdotH = max(dot(N, H), 0.0);

    // 粗糙度越小 → 高光块越小（阈值越高）
    const float threshold = mix(0.995, 0.90, clamp(lighting.roughness, 0.0, 1.0));
    const float spot = smoothstep(threshold, threshold + 0.004, NdotH);

    return vec3(spot * NdotL) * (1.0 - lighting.roughness * 0.5);
}

#endif // SPECULAR_TOON_STEP_GLSL
