#ifndef LIGHTING_LAMBERT_GLSL
#define LIGHTING_LAMBERT_GLSL

#include "common/surface_interface.glsl"

vec3 EvalLighting(SurfaceOutput surface, vec3 viewDir, vec3 lightDir, vec3 lightColor)
{
    vec3 N = surface.normal;
    vec3 L = lightDir;

    float NdotL = max(dot(N, L), 0.0);
    return surface.baseColor * lightColor * NdotL;
}

#endif
