// @ulre begin
// @ulre name direct_toon
// @ulre kind Utility
// @ulre priority 0
// @ulre uses lighting_interface
// @ulre end
// Direct Lighting — Toon / Stylized NPR Shading
#ifndef DIRECT_TOON_GLSL
#define DIRECT_TOON_GLSL

#include "common/lighting_interface.glsl"

vec3 EvalDirectLighting(
    SurfaceOutput surf,
    NTBSpace ntb,
    vec3 viewDir,
    vec3 lightDir,
    vec3 lightColor
) {
    vec3 N = ntb.N;
    vec3 V = viewDir;
    vec3 L = lightDir;
    vec3 H = normalize(V + L);

    float NdotL = dot(N, L);
    // 卡通 3 阶梯漫反射阈值
    float toonStep = smoothstep(0.0, 0.05, NdotL) * 0.5 + smoothstep(0.4, 0.45, NdotL) * 0.5;
    vec3 diffuse = surf.baseColor * mix(0.2, 1.0, toonStep);

    // 卡通硬边界高光 + 菲尼尔
    float NdotH = max(dot(N, H), 0.0);
    float specStep = smoothstep(0.85, 0.88, NdotH);
    vec3 F0 = mix(vec3(surf.fresnel), surf.baseColor, surf.metallic);
    vec3 F = ULRE_LIT_F_Schlick(max(dot(V, H), 0.0), F0);
    vec3 specular = vec3(specStep) * F * (1.0 - surf.roughness);

    return (diffuse + specular) * lightColor;
}

#endif // DIRECT_TOON_GLSL
