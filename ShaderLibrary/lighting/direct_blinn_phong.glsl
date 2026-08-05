// @ulre begin
// @ulre name direct_blinn_phong
// @ulre kind Utility
// @ulre priority 0
// @ulre uses lighting_interface
// @ulre end
// Direct Lighting — Blinn-Phong + Half-Lambert
#ifndef DIRECT_BLINN_PHONG_GLSL
#define DIRECT_BLINN_PHONG_GLSL

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

    // Half-Lambert Diffuse
    float NdotL = dot(N, L);
    float halfLambert = NdotL * 0.5 + 0.5;
    vec3 diffuse = surf.baseColor * (halfLambert * halfLambert);

    // Blinn-Phong Specular
    float NdotH = max(dot(N, H), 0.0);
    float shininess = mix(8.0, 128.0, 1.0 - surf.roughness);
    float specFactor = pow(NdotH, shininess);

    vec3 F0 = mix(vec3(surf.fresnel), surf.baseColor, surf.metallic);
    vec3 F = ULRE_LIT_F_Schlick(max(dot(V, H), 0.0), F0);
    vec3 specular = F * specFactor;

    return (diffuse + specular) * lightColor;
}

#endif // DIRECT_BLINN_PHONG_GLSL
