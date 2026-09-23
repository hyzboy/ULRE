// @ulre begin
// @ulre name pcf_shadow
// @ulre kind Utility
// @ulre priority 0
// @ulre slot shadow_provider
// @ulre provides_capability shadow_factor
// @ulre uses surface_interface
// @ulre uses bindless_textures
// @ulre uses scene_ubo
// @ulre end

#ifndef PCF_SHADOW_GLSL
#define PCF_SHADOW_GLSL

#include "common/surface_interface.glsl"
#include "common/bindless_textures.glsl"
#include "ubo/scene_ubo.glsl"

#ifndef ShadowPCFSampler
#define ShadowPCFSampler 4u
#endif

float EvalCascadePCF(uint c, vec3 worldPos, vec3 light_ndc, vec2 shadow_uv)
{
    const vec2  texel      = shadow.cascades[c].inv_shadow_map_size;
    const float pcf_radius = shadow.cascades[c].shadow_params.y;
    const float bias       = shadow.cascades[c].shadow_params.x;

    // 环形寻址（Toroidal Clipmap / 滚动缓存）：
    // 若 cache_offset 非零，通过 offset 偏移并求余 fract() 映射回物理纹理坐标
    const vec2 offset_uv = vec2(shadow.cascades[c].cache_offset.xy) * texel;
    const vec2 phys_uv   = fract(shadow_uv + offset_uv);

    const float current_depth = light_ndc.z + bias;
    const float layer         = float(shadow.cascades[c].shadow_tex.y);
    const uint  tex_handle    = shadow.cascades[c].shadow_tex.x;

    float lit = 0.0;
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            const vec2 tap = fract(phys_uv + vec2(float(dx), float(dy)) * (texel * pcf_radius));
            lit += Sample2DArrayShadow(tex_handle, ShadowPCFSampler, tap, layer, current_depth);
        }
    }

    const float unshadowed = lit * (1.0 / 9.0);
    return mix(shadow.cascades[c].shadow_params.z, 1.0, unshadowed);
}

float EvalPCFShadow(vec3 worldPos)
{
    if (shadow.shadow_tex.x == 0u)
        return 1.0;

    // 多级级联 CSM 模式 (csm_params.x > 0)
    if (shadow.csm_params.x > 0u)
    {
        const uint cascade_count = min(shadow.csm_params.x, 4u);
        for (uint c = 0u; c < cascade_count; ++c)
        {
            if (shadow.cascades[c].shadow_tex.x == 0u)
                continue;

            const vec4 light_clip = shadow.cascades[c].shadow_vp * vec4(worldPos, 1.0);
            if (light_clip.w <= 0.0)
                continue;

            const vec3 light_ndc = light_clip.xyz / light_clip.w;
            if (light_ndc.z < 0.0 || light_ndc.z > 1.0)
                continue;

            const vec2 shadow_uv = vec2(0.5) + 0.5 * light_ndc.xy;
            const vec2 texel = shadow.cascades[c].inv_shadow_map_size;
            const float pcf_radius = shadow.cascades[c].shadow_params.y;
            const float margin = texel.x * (pcf_radius + 1.0);

            if (shadow_uv.x >= margin && shadow_uv.x <= (1.0 - margin) &&
                shadow_uv.y >= margin && shadow_uv.y <= (1.0 - margin))
            {
                return EvalCascadePCF(c, worldPos, light_ndc, shadow_uv);
            }
        }
        return 1.0;
    }

    // 单级阴影回退路径（csm_params.x == 0）
    const vec4 light_clip = shadow.shadow_vp * vec4(worldPos, 1.0);
    if (light_clip.w <= 0.0)
        return 1.0;

    const vec3 light_ndc = light_clip.xyz / light_clip.w;

    // 检查是否在 reversed-Z 深度范围内 [0, 1]
    if (light_ndc.z < 0.0 || light_ndc.z > 1.0)
        return 1.0;

    const vec2 shadow_uv = vec2(0.5) + 0.5 * light_ndc.xy;

    const float bias       = shadow.shadow_params.x;
    const float pcf_radius = shadow.shadow_params.y;
    const vec2  texel      = shadow.inv_shadow_map_size;

    const float margin = texel.x * (pcf_radius + 1.0);
    if (shadow_uv.x < margin || shadow_uv.x > (1.0 - margin) ||
        shadow_uv.y < margin || shadow_uv.y > (1.0 - margin))
    {
        return 1.0;
    }

    // Reversed-Z 下深度比较：closer to light has greater Z
    // current_depth + bias 作为参考深度，硬件比较采样器（GreaterOrEqual）
    // 满足 ref >= depth 返回 1.0（未遮挡，受光），否则返回 0.0（被遮挡）
    const float current_depth = light_ndc.z + bias;
    const float layer         = float(shadow.shadow_tex.y);
    const uint  tex_handle    = shadow.shadow_tex.x;

    float lit = 0.0;
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            const vec2 tap = shadow_uv + vec2(float(dx), float(dy)) * (texel * pcf_radius);
            lit += Sample2DArrayShadow(tex_handle, ShadowPCFSampler, tap, layer, current_depth);
        }
    }

    const float unshadowed = lit * (1.0 / 9.0);
    return mix(shadow.shadow_params.z, 1.0, unshadowed);
}

float GetShadowFactor(SurfaceInput surface)
{
    return EvalPCFShadow(surface.worldPos);
}

#endif // PCF_SHADOW_GLSL
