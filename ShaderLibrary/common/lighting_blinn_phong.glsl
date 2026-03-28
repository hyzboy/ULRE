#ifndef LIGHTING_BLINN_PHONG_GLSL
#define LIGHTING_BLINN_PHONG_GLSL

#include "common/surface_interface.glsl"

vec3 EvalLighting(SurfaceOutput surface, vec3 viewDir, vec3 lightDir, vec3 lightColor)
{
    vec3 N = surface.normal;
    vec3 V = viewDir;
    vec3 L = lightDir;

    float h         = dot(N, L) * 0.5 + 0.5;
    float hl        = h * h;
    vec3  H         = normalize(V + L);
    float shininess = mix(256.0, 8.0, surface.roughness);
    float spec      = pow(max(dot(N, H), 0.0), shininess);
    float specScale = surface.metallic * (1.0 - surface.roughness * 0.9);
    vec3  specColor = mix(vec3(spec), surface.baseColor * spec, surface.metallic);
    return surface.baseColor * hl * lightColor + specColor * specScale * lightColor;
}

#endif
