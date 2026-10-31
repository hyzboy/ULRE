// @ulre begin
// @ulre name output_policy_forward
// @ulre kind Utility
// @ulre priority 0
// @ulre slot output_policy
// @ulre provides_capability forward_output
// @ulre uses surface_interface
// @ulre uses lighting_interface
// @ulre uses pass_environment_interface
// @ulre end
// Forward Lit compositor input assembly.
// This module resolves authored material data and scene data into LightingInput.
// It does not implement a lighting algorithm.

#ifndef FORWARD_LIGHTING_GLSL
#define FORWARD_LIGHTING_GLSL

#include "module/interface/surface_interface.glsl"
#include "module/interface/lighting_interface.glsl"
#include "module/interface/pass_environment_interface.glsl"

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
    // AO 契约：ambient_occlusion_provider 槽的模块提供
    // `float GetAmbientOcclusion(SurfaceInput)`。该槽现在是**所有** forward 光照
    // 模板的必需槽（不需要 AO 的模板用恒等实现 identity_ao 补位），因此这里无条件
    // 接线 —— 不再有 HGL_USE_AMBIENT_OCCLUSION_PROVIDER 分支。
    lighting.ao *= GetAmbientOcclusion(si);
    lighting.emissive = surf.emissive;
    lighting.alpha = surf.alpha;

    lighting.mainLightDir = GetMainLightDir();
    lighting.mainLightColor = GetMainLightColor();
    lighting.reflectionColor = vec3(0.0);
    // D3：阴影接收开关与局部偏差倍率随 per-draw 行到达（data_index = 本 FS 的
    // 材质数据行号）。不接收阴影的图元在此恒得 1.0。
    lighting.shadowFactor = GetShadowFactor(si, data_index);
    return lighting;
}

// ── Pass Environment 契约实现（ForwardLit 环境）─────────────────────────────
// 领域/模板只认这一个入口：EvalPassEnvironment(env)。
// “本 Pass 用哪个环境” = 本模块由 output_policy 槽选中；换环境 = 换一个模块，
// 模板与领域源码一行不动。本模块仍只做装配，真正的 BRDF 由 LightingModel 槽
// 的模块（lighting/*.glsl 的 EvalLighting）提供。
vec4 EvalPassEnvironment(PassEnvironmentInput env)
{
    const LightingInput lighting = BuildForwardLightingInput(
        env.surface, env.geom, env.dataIndex);
    return EvalLighting(lighting);
}

#endif // FORWARD_LIGHTING_GLSL
