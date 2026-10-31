// @ulre begin
// @ulre name lighting_model_toon
// @ulre kind Utility
// @ulre priority 0
// @ulre slot lighting_model
// @ulre requires_capability surface_base_color|surface_normal|surface_metallic|surface_roughness|direct_diffuse|direct_specular|ambient_diffuse
// @ulre uses lighting_interface
// @ulre end
// 光照合成槽 — 卡通单元化合成（NPR）
//
// 与 forward_pbr 同签名（LightingInput → vec4），只是把“直接光 + 间接光”
// 的连续相加换成按亮度分的两三段阶。它照样调用 diffuse_* / specular_* /
// ambient_* 三个槽——**换合成方式不等于换光照模型**：
//   diffuse_lambert + specular_cook_torrance + forward_toon
// 是合法的（写实高光 + 卡通明暗），反之亦然。
#ifndef FORWARD_TOON_GLSL
#define FORWARD_TOON_GLSL

#include "module/interface/lighting_interface.glsl"

vec4 EvalLighting(LightingInput lighting)
{
    const vec3 direct = (EvalDiffuseLight(lighting) + EvalSpecularLight(lighting))
                      * lighting.mainLightColor
                      * lighting.shadowFactor;
    const vec3 ambient = EvalIndirectLighting(lighting);

    const vec3 lit = direct + ambient;
    const float luminance = dot(lit, vec3(0.2126, 0.7152, 0.0722));

    // 三段阶：暗部压平、中间调抬亮、高光截断
    const float shadow_step    = smoothstep(0.05, 0.12, luminance);
    const float highlight_step = smoothstep(0.55, 0.62, luminance);
    const float band = shadow_step * 0.55 + highlight_step * 0.45;

    // 量化只作用在亮度上，色度仍来自真实光照（彩色光源不会变成灰）
    const vec3 chroma = lit / max(luminance, 1e-4);

    return vec4(lighting.baseColor * chroma * band + lighting.emissive,
                lighting.alpha);
}

#endif // FORWARD_TOON_GLSL
