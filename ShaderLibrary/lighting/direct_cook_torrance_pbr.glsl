// @ulre begin
// @ulre name direct_cook_torrance_pbr
// @ulre kind Utility
// @ulre priority 0
// @ulre uses lighting_interface
// @ulre end
// Direct Lighting — Cook-Torrance GGX PBR
#ifndef DIRECT_COOK_TORRANCE_PBR_GLSL
#define DIRECT_COOK_TORRANCE_PBR_GLSL

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

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 1e-4);
    vec3  H     = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    float alpha2 = surf.roughness * surf.roughness * surf.roughness * surf.roughness;
    float D      = ULRE_LIT_D_GGX(NdotH, alpha2);
    float G      = ULRE_LIT_G_Smith(NdotV, NdotL, surf.roughness);
    vec3  F0     = mix(vec3(surf.fresnel), surf.baseColor, surf.metallic);
    vec3  F      = ULRE_LIT_F_Schlick(VdotH, F0);

    vec3 kd       = (1.0 - F) * (1.0 - surf.metallic);
    vec3 diffuse  = kd * surf.baseColor / 3.14159265 * NdotL;
    vec3 specular = D * G * F / max(4.0 * NdotV * NdotL, 1e-4) * NdotL;

    return (diffuse + specular) * lightColor;
}

#endif // DIRECT_COOK_TORRANCE_PBR_GLSL
