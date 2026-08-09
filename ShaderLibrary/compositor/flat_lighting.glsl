// @ulre begin
// @ulre name flat_lighting
// @ulre kind Utility
// @ulre priority 0
// @ulre uses surface_interface
// @ulre uses lighting_interface
// @ulre end

#ifndef FLAT_LIGHTING_GLSL
#define FLAT_LIGHTING_GLSL

#include "common/surface_interface.glsl"
#include "common/lighting_interface.glsl"

LightingInput BuildForwardLightingInput(
    SurfaceOutput surface,
    SurfaceInput surfaceInput)
{
    LightingInput lighting;
    lighting.baseColor = surface.baseColor;
    lighting.normal = normalize(surface.normal);
    lighting.viewDir = normalize(surfaceInput.viewDir);
    lighting.metallic = surface.metallic;
    lighting.roughness = surface.roughness;
    lighting.fresnel = surface.fresnel;
    lighting.ao = surface.ao;
    lighting.emissive = surface.emissive;
    lighting.alpha = surface.alpha;
    lighting.mainLightDir = vec3(0.0, 0.0, 1.0);
    lighting.mainLightColor = vec3(0.0);
    lighting.ambientColor = vec3(0.0);
    lighting.reflectionColor = vec3(0.0);
    return lighting;
}

#endif
