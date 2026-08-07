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
    LightingInput lighting
) {
    vec3 N = normalize(lighting.normal);
    vec3 V = lighting.viewDir;
    float NdotV = max(dot(N, V), 0.0);

    vec3 F0 = mix(vec3(lighting.fresnel), lighting.baseColor, lighting.metallic);
    vec3 F = ULRE_LIT_F_Schlick(NdotV, F0);
    vec3 kd = (1.0 - F) * (1.0 - lighting.metallic);

    vec3 diffuseAmbient = kd * lighting.baseColor * lighting.ambientColor;
    vec3 specularAmbient = F * lighting.ambientColor * (1.0 - lighting.roughness);

    return (diffuseAmbient + specularAmbient) * lighting.ao;
}

#endif // INDIRECT_PBR_AMBIENT_GLSL
