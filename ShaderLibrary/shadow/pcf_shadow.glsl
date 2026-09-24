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

float EvalCascadePCF(uint c, vec3 light_ndc, vec2 shadow_uv)
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

// 对单个级联求阴影因子。
//
// out_edge_dist 返回片元在 UV 空间到级联方框边界的距离（可为负，表示已越界）：
//   > 0  该级联在本片元处有数据，可参与边界混合
//   < 0  该级联在本片元处**没有可用数据**（片元落在该级联的深度窗口之外）。
//        调用者必须改试相邻级联，绝不能据此判定为"受光"，否则大片地面会被判成无阴影。
//
// UV 越界时按半纹素 clamp 采样，而不是回退 return 1.0：级联方框是包围球的
// XY 外接方框，方框外的片元仍是可见几何体，直接把最近纹素的深度用上比判成
// 全受光更接近真实，也不会产生整片"阴影消失"。
float EvalCascadeShadowAt(uint c, vec3 worldPos, out float out_edge_dist)
{
    out_edge_dist = -1.0;

    const vec4 light_clip = shadow.cascades[c].shadow_vp * vec4(worldPos, 1.0);
    if (!(light_clip.w > 0.0))          // 同时挡住 w<=0 与 NaN
        return 1.0;

    const vec3 light_ndc = light_clip.xyz / light_clip.w;

    // 片元落在该级联的深度窗口之外 → 本级联在此处无数据
    if (light_ndc.z < 0.0 || light_ndc.z > 1.0)
        return 1.0;

    const vec2 uv_raw     = vec2(0.5) + 0.5 * light_ndc.xy;
    const vec2 texel      = shadow.cascades[c].inv_shadow_map_size;
    const vec2 half_texel = texel * 0.5;

    out_edge_dist = min(min(uv_raw.x, 1.0 - uv_raw.x),
                        min(uv_raw.y, 1.0 - uv_raw.y));

    const vec2 uv = clamp(uv_raw, half_texel, vec2(1.0) - half_texel);

    return EvalCascadePCF(c, light_ndc, uv);
}

float EvalPCFShadow(vec3 worldPos)
{
    if (shadow.shadow_tex.x == 0u)
        return 1.0;

    // 多级级联 CSM 模式 (csm_params.x > 0)
    if (shadow.csm_params.x > 0u)
    {
        const uint cascade_count = min(shadow.csm_params.x, 4u);

        // ── 选级：按"相机 → 片元"的前向深度与每级 cascade_params.y(=split_far) 匹配 ──
        // 不能用"片元投影 UV 是否落在某一级联方框内"来选级：方框外仍可能有大量
        // 可见几何体（典型是 500x500 的地面），旧实现会把它们全部落到循环末尾的
        // return 1.0（恒受光），表现为"地面大片不接收阴影"。
        // 级联方框是按该级段视锥包围球构建的，因此"前向深度 <= split_far 的视锥内
        // 片元必然落在该级联方框内"，按深度选级与方框构建完全自洽。
        const vec3  cam_fwd_raw = camera.view_line;
        const float cam_fwd_len = length(cam_fwd_raw);

        // 前向深度与 split_far 同一度量（级联正是沿 cam_forward 分段的）。
        // view_line 异常（未初始化）时退化为欧氏距离，避免 NaN 让所有比较失效。
        const float view_depth = (cam_fwd_len > 1.0e-6)
                               ? dot(worldPos - camera.camera_world_pos, cam_fwd_raw / cam_fwd_len)
                               : length(worldPos - camera.camera_world_pos);

        uint selected = 0xFFFFFFFFu;    // 0xFFFFFFFF = 尚无有效级联
        for (uint c = 0u; c < cascade_count; ++c)
        {
            if (shadow.cascades[c].shadow_tex.x == 0u)
                continue;

            selected = c;
            if (view_depth <= shadow.cascades[c].cascade_params.y)
                break;
        }

        if (selected == 0xFFFFFFFFu)    // 所有级联都没有阴影贴图
            return 1.0;

        float edge_dist = -1.0;
        float shadow_factor = EvalCascadeShadowAt(selected, worldPos, edge_dist);

        if (edge_dist < 0.0)
        {
            // 所选级联的深度窗口不覆盖本片元（片元高出/低于该级联的深度范围）：
            // 依次改试相邻级联，都没有数据才判定为受光。
            if (selected + 1u < cascade_count)
            {
                float other_edge = -1.0;
                const float other = EvalCascadeShadowAt(selected + 1u, worldPos, other_edge);
                if (other_edge >= 0.0)
                    return other;
            }

            if (selected > 0u)
            {
                float other_edge = -1.0;
                const float other = EvalCascadeShadowAt(selected - 1u, worldPos, other_edge);
                if (other_edge >= 0.0)
                    return other;
            }

            return 1.0;
        }

        // ── 级联边界混合 ──
        // 取两种"本级联即将失效"判据中较小者，避免出现硬边：
        //   1) 距离：接近本级 cascade_params.y(=split_far) 时交给下一级（更远、覆盖更大）
        //   2) UV：接近本级方框边界时（cascade_params.z=blend_width，UV 空间比例）
        // 最后一级向受光淡出，避免阴影在覆盖范围外被硬切。
        const float blend_width = shadow.cascades[selected].cascade_params.z;

        if (blend_width > 0.0)
        {
            const float split_near = shadow.cascades[selected].cascade_params.x;
            const float split_far  = shadow.cascades[selected].cascade_params.y;
            const float band       = blend_width * max(split_far - split_near, 1.0e-3);

            const float fade_depth = clamp((split_far - view_depth) / band, 0.0, 1.0);
            const float fade_uv    = smoothstep(0.0, blend_width, edge_dist);
            const float blend      = min(fade_depth, fade_uv);

            if (blend < 1.0)
            {
                if (selected + 1u < cascade_count)
                {
                    float next_edge = -1.0;
                    const float next_shadow = EvalCascadeShadowAt(selected + 1u, worldPos, next_edge);
                    if (next_edge >= 0.0)
                        return mix(next_shadow, shadow_factor, blend);
                }
                else
                {
                    return mix(1.0, shadow_factor, blend);
                }
            }
        }

        return shadow_factor;
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
