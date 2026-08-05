// @ulre begin
// @ulre name indirect_pbr_ambient
// @ulre kind Utility
// @ulre priority 0
// @ulre uses lighting_interface
// @ulre end
// Indirect Lighting — PBR Ambient / Hemisphere Light
#ifndef INDIRECT_PBR_AMBIENT_GLSL
#define INDIRECT_PBR_AMBIENT_GLSL

#include "common/lighting_interface.glsl"

vec3 EvalIndirectLighting(
    SurfaceOutput surf,
    NTBSpace ntb,
    vec3 viewDir,
    vec3 skyAmbientColor
) {
    vec3 N = ntb.N;
    vec3 V = viewDir;
    float NdotV = max(dot(N, V), 0.0);

    vec3 F0 = mix(vec3(surf.fresnel), surf.baseColor, surf.metallic);
    vec3 F = ULRE_LIT_F_Schlick(NdotV, F0);
    vec3 kd = (1.0 - F) * (1.0 - surf.metallic);

    vec3 diffuseAmbient = kd * surf.baseColor * skyAmbientColor;
    vec3 specularAmbient = F * skyAmbientColor * (1.0 - surf.roughness);

    return (diffuseAmbient + specularAmbient) * surf.ao;
}

#endif // INDIRECT_PBR_AMBIENT_GLSL
