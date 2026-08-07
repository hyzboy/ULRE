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
    LightingInput lighting
) {
    vec3 N = normalize(lighting.normal);
    vec3 V = lighting.viewDir;
    vec3 L = lighting.mainLightDir;
    vec3 H = normalize(V + L);

    // Half-Lambert Diffuse
    float NdotL = dot(N, L);
    float halfLambert = NdotL * 0.5 + 0.5;
    vec3 diffuse = lighting.baseColor * (halfLambert * halfLambert);

    // Blinn-Phong Specular
    float NdotH = max(dot(N, H), 0.0);
    float shininess = mix(8.0, 128.0, 1.0 - lighting.roughness);
    float specFactor = pow(NdotH, shininess);

    vec3 F0 = mix(vec3(lighting.fresnel), lighting.baseColor, lighting.metallic);
    vec3 F = ULRE_LIT_F_Schlick(max(dot(V, H), 0.0), F0);
    vec3 specular = F * specFactor;

    return (diffuse + specular) * lighting.mainLightColor;
}

#endif // DIRECT_BLINN_PHONG_GLSL
