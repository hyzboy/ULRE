// @ulre begin
// @ulre name forward_lighting
// @ulre kind Utility
// @ulre priority 0
// @ulre slot output_policy
// @ulre provides_capability forward_output
// @ulre uses surface_interface
// @ulre uses lighting_interface
// @ulre end
// Forward Lit compositor input assembly.
// This module resolves authored material data and scene data into LightingInput.
// It does not implement a lighting algorithm.

#ifndef FORWARD_LIGHTING_GLSL
#define FORWARD_LIGHTING_GLSL

#include "common/surface_interface.glsl"
#include "common/lighting_interface.glsl"

// D3：data_index = 本 FS 的材质数据行号（per-draw 行表），用于读该图元的接收侧阴影参数。
LightingInput BuildForwardLightingInput(SurfaceOutput surf, SurfaceInput si, uint data_index)
{
    LightingInput lighting;
    lighting.baseColor = surf.baseColor;
    lighting.normal = normalize(surf.normal);
    lighting.viewDir = normalize(si.viewDir);
    lighting.metallic = surf.metallic;
    lighting.roughness = surf.roughness;
    lighting.fresnel = surf.fresnel;
    lighting.ao = surf.ao;
    lighting.emissive = surf.emissive;
    lighting.alpha = surf.alpha;

    lighting.mainLightDir = GetSkyMainLightDir();
    lighting.mainLightColor = GetSkyMainLightColor();
    lighting.ambientColor = GetSkyAmbientColor();
    lighting.reflectionColor = vec3(0.0);
    // D3：阴影接收开关与局部偏差倍率随 per-draw 行到达（data_index = 本 FS 的
    // 材质数据行号）。不接收阴影的图元在此恒得 1.0。
    lighting.shadowFactor = GetShadowFactor(si, data_index);
    return lighting;
}

#endif // FORWARD_LIGHTING_GLSL
