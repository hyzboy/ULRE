// @ulre begin
// @ulre name lighting_model_pbr
// @ulre kind Utility
// @ulre priority 0
// @ulre slot lighting_model
// @ulre requires_capability surface_base_color|surface_normal|surface_metallic|surface_roughness|direct_diffuse|direct_specular|ambient_diffuse
// @ulre uses lighting_interface
// @ulre end
// Default forward lighting compositor.
// 漫反射与高光来自两个**互相独立**的可插槽（diffuse_light_provider /
// specular_light_provider），间接光来自第三个槽（ambient_light_provider）。
// 三者都只返回未调制的辐射项，调制（光源色 + 阴影）只在这里做一次。
// “无高光/无漫反射/无间接光”都是把它们换成 *_none 实现，不是 #if。

#ifndef FORWARD_PBR_GLSL
#define FORWARD_PBR_GLSL

#include "module/interface/lighting_interface.glsl"

vec4 EvalLighting(LightingInput lighting)
{
    const vec3 directColor =
        (EvalDiffuseLight(lighting) + EvalSpecularLight(lighting))
        * lighting.mainLightColor
        * lighting.shadowFactor;

    vec3 indirectColor = EvalIndirectLighting(lighting);

    // 反射项（SSR/env 等外部提供的 prefiltered radiance；forward policy 当前置 0）
    // —— Filament 语义：Fr = radiance * specularDFG * energyCompensation
    //    specularDFG = mix(dfg.xxx, dfg.yyy, F0) = F0*dfg.x + (1-F0)*dfg.y
    //    energyComp  = 1 + F0*(1/dfg.y - 1)   （Kulla & Conty 2017 多散射近似）
    const vec3 N = normalize(lighting.normal);
    const float NdotV = max(dot(N, lighting.viewDir), 1e-4);
    const vec3 F0 = mix(vec3(lighting.fresnel), lighting.baseColor, lighting.metallic);
    const vec2 dfg = ULRE_LIT_EnvDFGApprox(NdotV, lighting.roughness);
    const vec3 E = F0 * dfg.x + (1.0 - F0) * dfg.y;
    const vec3 energyComp = 1.0 + F0 * (1.0 / dfg.y - 1.0);
    indirectColor += lighting.reflectionColor * E * energyComp;

    return vec4(
        directColor + indirectColor + lighting.emissive,
        lighting.alpha);
}

#endif // FORWARD_PBR_GLSL
