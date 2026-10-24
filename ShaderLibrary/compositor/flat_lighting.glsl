// @ulre begin
// @ulre name flat_lighting
// @ulre kind Utility
// @ulre priority 0
// @ulre slot output_policy
// @ulre provides_capability forward_output
// @ulre uses surface_interface
// @ulre uses lighting_interface
// @ulre uses pass_environment_interface
// @ulre uses forward_flat
// @ulre end

#ifndef FLAT_LIGHTING_GLSL
#define FLAT_LIGHTING_GLSL

#include "common/surface_interface.glsl"
#include "common/lighting_interface.glsl"
#include "common/pass_environment_interface.glsl"
// 光照算法由本环境模块自己引入，而不是由模板硬编码：
// 模板只调用 EvalPassEnvironment，不再认识任何 lighting/*.glsl。
#include "lighting/forward_flat.glsl"

// flat 合成器不采样阴影，data_index 仅为与 forward_lighting 保持同一签名
// （模板统一传 materialDataIndex）。
LightingInput BuildForwardLightingInput(
    SurfaceOutput surface,
    SurfaceInput surfaceInput,
    uint data_index)
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
    lighting.reflectionColor = vec3(0.0);
    lighting.shadowFactor = 1.0;
    return lighting;
}

// ── Pass Environment 契约实现（无光/平光环境）───────────────────────────────
// 与 forward_lighting 同签名，故 output_policy 槽可以在两者之间互换，
// 而模板与领域源码不需要任何改动。
vec4 EvalPassEnvironment(PassEnvironmentInput env)
{
    const LightingInput lighting = BuildForwardLightingInput(
        env.surface, env.geom, env.dataIndex);
    return EvalLighting(lighting);
}

#endif
