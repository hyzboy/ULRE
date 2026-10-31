// @ulre begin
// @ulre name diffuse_light_toon
// @ulre kind Utility
// @ulre priority 0
// @ulre slot diffuse_light_provider
// @ulre provides_capability direct_diffuse
// @ulre uses lighting_interface
// @ulre end
// 漫反射槽 — 卡通两段阶（NPR）
//
// 非物理模型，故意不做能量守恒：这里要的是那条硬边的明暗分界。
// 与 diffuse_lambert 同签名，可直接换装。
#ifndef DIFFUSE_TOON_GLSL
#define DIFFUSE_TOON_GLSL

#include "module/interface/lighting_interface.glsl"

vec3 EvalDiffuseLight(LightingInput lighting)
{
    const vec3 N = normalize(lighting.normal);
    const vec3 L = lighting.mainLightDir;

    // 半兰伯特把 [-1,1] 压到 [0,1]，避免背光面全黑
    const float ndl = dot(N, L) * 0.5 + 0.5;
    // 两段阶：分界附近用 smoothstep 留一点抗锯齿宽度，其余是平的
    const float band = smoothstep(0.45, 0.55, ndl);

    return lighting.baseColor * band * (1.0 - lighting.metallic);
}

#endif // DIFFUSE_TOON_GLSL
