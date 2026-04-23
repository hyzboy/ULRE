#ifndef LIGHTING_PBR_GLSL
#define LIGHTING_PBR_GLSL

#include "common/surface_interface.glsl"
#include "util/pbr_brdf.glsl"

vec3 EvalLighting(SurfaceOutput surface, vec3 viewDir, vec3 lightDir, vec3 lightColor)
{
    vec3 N = surface.normal;
    vec3 V = viewDir;
    vec3 L = lightDir;

    float NdotL  = max(dot(N, L), 0.0);
    float NdotV  = max(dot(N, V), 1e-4);
    vec3  H      = normalize(V + L);
    float NdotH  = max(dot(N, H), 0.0);
    float VdotH  = max(dot(V, H), 0.0);

    float alpha2 = surface.roughness * surface.roughness * surface.roughness * surface.roughness;
    float D      = PBR_D_GGX(NdotH, alpha2);
    float G      = PBR_G_Smith(NdotV, NdotL, surface.roughness);
    vec3  F0     = mix(vec3(0.04), surface.baseColor, surface.metallic);
    vec3  F      = PBR_F_Schlick(VdotH, F0);

    vec3 kd       = (1.0 - F) * (1.0 - surface.metallic);
    vec3 diffuse  = kd * surface.baseColor / 3.14159265 * NdotL;
    vec3 specular = D * G * F / max(4.0 * NdotV * NdotL, 1e-4) * NdotL;

    return (diffuse + specular) * lightColor;
}

#endif
