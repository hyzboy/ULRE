// @ulre begin
// @ulre name lighting_model_pbr
// @ulre kind Utility
// @ulre priority 0
// @ulre slot lighting_model
// @ulre requires_capability surface_base_color|surface_normal|surface_metallic|surface_roughness|direct_diffuse|direct_specular|ambient_diffuse|direct_clearcoat|direct_sheen|direct_subsurface
// @ulre uses lighting_interface
// @ulre end
// Default forward lighting compositor.
// 漫反射与高光来自两个**互相独立**的可插槽（diffuse_light_provider /
// specular_light_provider），间接光来自第三个槽（ambient_light_provider）。
// 三者都只返回未调制的辐射项，调制（光源色 + 阴影）只在这里做一次。
// “无高光/无漫反射/无间接光”都是把它们换成 *_none 实现，不是 #if。
//
// 高级槽（清漆 / 绒面 / 次表面）同样**无条件**组合：
//   - 基线绑定 *_none（恒 0）⇒ 加法项折叠消失、衰减乘数折叠为 1.0；
//   - 启用高级材质只是把槽换成 filament 实现，本文件不再改动。
// 能量守恒近似（与 Filament 语义一致，简化处已注明）：
//   - 绒面按 sheenEnergyLoss 从漫反射中扣除；
//   - 清漆按 F_Schlick(clearCoat, 0, NoV) 衰减整个基础层。

#ifndef FORWARD_PBR_GLSL
#define FORWARD_PBR_GLSL

#include "module/interface/lighting_interface.glsl"

vec4 EvalLighting(LightingInput lighting)
{
    const vec3  N     = normalize(lighting.normal);
    const float NdotV = max(dot(N, lighting.viewDir), 1e-4);
    const vec3  L     = lighting.mainLightDir;
    const float NdotL = max(dot(N, L), 0.0);

    // ── 直接光基础层：两个独立槽的未调制辐射项 ──────────────────────────
    vec3 diffuse  = EvalDiffuseLight(lighting);
    vec3 specular = EvalSpecularLight(lighting);

    // 绒面能量守恒：从漫反射扣除（基线 sheenColor=0 → 三目折叠为恒 1.0 乘子）
    diffuse *= (dot(lighting.sheenColor, lighting.sheenColor) > 0.0)
        ? (1.0 - ULRE_LIT_SheenEnergyLoss(
               NdotV, max(NdotL, 1e-4),
               lighting.sheenRoughness * lighting.sheenRoughness))
        : 1.0;

    // 清漆：基础层被 (1 - F_Schlick(clearCoat, 0, NoV)) 衰减；
    // clearCoat = 0 时该乘子精确等于 1.0，常量折叠无副作用。
    const float clearCoatNoV = ULRE_LIT_F_Schlick_S(lighting.clearCoat, 0.0, NdotV);
    const vec3 base = (diffuse + specular) * (1.0 - clearCoatNoV);

    // 高级加法槽（none 实现恒 0，基线整链 DCE）
    const vec3 advanced = EvalClearCoatLight(lighting)
                        + EvalSheenLight(lighting)
                        + EvalSubsurfaceLight(lighting);

    const vec3 directColor =
        (base + advanced)
        * lighting.mainLightColor
        * lighting.shadowFactor;

    vec3 indirectColor = EvalIndirectLighting(lighting);

    // 反射项（SSR/env 等外部提供的 prefiltered radiance；forward policy 当前置 0）
    // —— Filament 语义：Fr = radiance * specularDFG * energyCompensation
    //    specularDFG = mix(dfg.xxx, dfg.yyy, F0) = F0*dfg.x + (1-F0)*dfg.y
    //    energyComp  = 1 + F0*(1/dfg.y - 1)   （Kulla & Conty 2017 多散射近似）
    const vec3 F0 = mix(vec3(lighting.fresnel), lighting.baseColor, lighting.metallic);
    const vec2 dfg = ULRE_LIT_EnvDFGApprox(NdotV, lighting.roughness);
    const vec3 E = F0 * dfg.x + (1.0 - F0) * dfg.y;
    const vec3 energyComp = 1.0 + F0 * (1.0 / max(dfg.x, 0.1) - 1.0);
    indirectColor += lighting.reflectionColor * E * energyComp;

    return vec4(
        directColor + indirectColor + lighting.emissive,
        lighting.alpha);
}

#endif // FORWARD_PBR_GLSL
