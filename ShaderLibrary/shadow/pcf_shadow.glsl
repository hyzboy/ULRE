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

// ── PCF 采样方式（编译期宏，由 ShaderGen 生成 GLSL 时注入）─────────────────
// 值 = Poisson 磁盘采样数：
//   0   → 旧版 3x3 网格盒式采样（保留以便 A/B 对比与回退）
//   > 0 → 单位圆 Poisson 磁盘采样（上限 32），叠加每片元旋转抖动
// 注入点：FragmentTemplateComposer::BuildFragmentDefines（模板带
// shadow_provider 槽时发射）。下面的默认值仅在单独编译本模块时生效。
#ifndef HGL_SHADOW_PCF_POISSON_TAPS
#define HGL_SHADOW_PCF_POISSON_TAPS 16
#endif

#if HGL_SHADOW_PCF_POISSON_TAPS > 0
// 32 点 Poisson 磁盘（固定种子生成，半径归一化到 1，最小间距 ≈ 0.27）。
// 采样范围与旧版 3x3 盒式一致：偏移量 = 磁盘坐标 * texel * pcf_radius。
const vec2 ShadowPoissonDisk[32] = vec2[32](
    vec2( 0.000000,  0.000000), vec2(-0.978611, -0.205718), vec2( 0.733448,  0.676034),
    vec2( 0.004679, -0.997756), vec2( 0.927512, -0.349982), vec2(-0.330630,  0.941093),
    vec2(-0.839914,  0.453569), vec2(-0.482138, -0.560177), vec2( 0.385253, -0.536410),
    vec2( 0.159607,  0.559925), vec2( 0.571619,  0.115694), vec2(-0.348539,  0.358100),
    vec2(-0.035266, -0.447095), vec2(-0.482025, -0.118002), vec2( 0.289812,  0.948986),
    vec2( 0.988193,  0.026413), vec2( 0.402792, -0.912604), vec2(-0.762932,  0.103883),
    vec2( 0.737857, -0.672531), vec2(-0.652872,  0.732887), vec2(-0.345053, -0.899931),
    vec2( 0.319079, -0.148446), vec2( 0.492496,  0.443917), vec2(-0.855182, -0.515254),
    vec2( 0.900179,  0.351873), vec2(-0.145322,  0.685199), vec2( 0.647886, -0.202118),
    vec2( 0.254332,  0.223268), vec2(-0.046856,  0.340706), vec2(-0.020054,  0.942472),
    vec2( 0.436034,  0.718961), vec2( 0.140202, -0.678706));

// 每片元旋转相位：gl_FragCoord 生成交错梯度噪声（Jimenez）再映射到 [0, 2π)。
// 不抖动时固定采样图案会在大平面上形成结构性走样。
float ShadowPoissonPhase(vec2 frag_coord)
{
    const float noise =
        fract(52.9829189 * fract(0.06711056 * frag_coord.x + 0.00583715 * frag_coord.y));
    return noise * 6.28318530717958648;
}

// Poisson 磁盘 PCF：返回受光比例 ∈ [0,1]（未乘 darkness）。
// wrap_uv = true 时对每次采样做 fract() 环形寻址（滚动缓存 Toroidal Clipmap，
// 仅当该级联 cache_offset 非零时由调用方开启）；false 时越界 tap 钳在贴图
// 边缘——fract 环绕会让边缘 tap 跳到贴图对侧取深度，留下一圈杂斑。
float EvalPoissonPCF(uint tex_handle, uint layer, vec2 uv, vec2 texel,
                     float radius, float ref_depth, bool wrap_uv, float phase)
{
    const float cos_phase = cos(phase);
    const float sin_phase = sin(phase);
    const vec2  scale     = texel * radius;

    float lit = 0.0;
    for (int i = 0; i < min(HGL_SHADOW_PCF_POISSON_TAPS, 32); ++i)
    {
        const vec2 p = ShadowPoissonDisk[i];
        const vec2 rotated = vec2(p.x * cos_phase - p.y * sin_phase,
                                  p.x * sin_phase + p.y * cos_phase);
        vec2 tap = uv + rotated * scale;
        tap = wrap_uv ? fract(tap)
                      : clamp(tap, vec2(0.0), vec2(1.0));
        lit += Sample2DArrayShadow(tex_handle, ShadowPCFSampler, tap, layer, ref_depth);
    }

    return lit * (1.0 / float(min(HGL_SHADOW_PCF_POISSON_TAPS, 32)));
}
#endif

// ── Normal-offset shadow mapping（编译期宏，由 ShaderGen 生成 GLSL 时注入）────
//   0 → 关闭：着色点原样送去采样，完全依赖深度 bias 压 acne
//   1 → 开启
// 注入点：FragmentTemplateComposer::BuildFragmentDefines（模板带 shadow_provider
// 槽时发射）。下面的默认值仅在单独编译本模块时生效。
//
// 宏管"有没有这段代码"，运行时强度管"用多大劲"：强度来自
// ShadowCascadeInfo::shadow_params.w（世界单位米，控制器把同一个值写进每一级，
// 故读级联 0 即可；未配置时为结构体默认值 0.0 ⇒ 等价于不偏移）。
// 宏关掉时连一次 normalize/dot 都不会留下。
#ifndef HGL_SHADOW_NORMAL_OFFSET
#define HGL_SHADOW_NORMAL_OFFSET 1
#endif

/// 法线偏移量的硬上限（世界单位米）。夹住 tan(θ) 在掠射角处的发散，
/// 否则 tan→+∞ 会把采样点甩到相邻物体背后，表现为"阴影脱离物体/漏光"。
const float SHADOW_NORMAL_OFFSET_MAX = 1.5;

#if HGL_SHADOW_NORMAL_OFFSET > 0
/// 沿几何法线外推着色点，返回用于**采样**的世界坐标。
///
///   offset = clamp(strength * tan(θ), 0, MAX)，θ = 几何法线与入射光的夹角
///   采样点 = world_pos + N * offset
///
/// 为什么是 tan(θ)：acne 的深度误差 ∝ 表面在一个纹素跨度内沿光轴的高度变化
/// ∝ tan(θ)。正面受光（θ≈0）几乎不需要偏移，掠射面（θ→90°）才需要大偏移。
/// 这恰好补上固定深度 bias 的短板——bias 与角度无关，为掠射面调大就会让所有
/// 正面一起"漏光"（peter-panning）；法线偏移只推斜射面，正面纹丝不动。
///
/// 用**几何**法线（SurfaceInput.worldNormal）而非着色法线：normal map 的扰动
/// 只改变外观、不改变真实轮廓，拿它做偏移会在法线花纹上抖出噪点。
///
/// 光方向取 sky.sun_direction —— 与本引擎其余阴影接收代码
/// （material/shadow_receiver_source.glsl）同源，其约定是 forward = -sun_direction，
/// 故 sun_direction 指向**光源**，正是这里要的 L。若某个场景把 CSM 光与
/// sun_direction 驱动成两个不同的方向，请把强度置 0 或关掉宏。
vec3 ShadowNormalOffsetPosition(vec3 world_pos, vec3 world_normal, float strength)
{
    if (strength <= 0.0)
        return world_pos;

    const float n_len = length(world_normal);
    if (n_len <= 1.0e-6)
        return world_pos;

    const vec3  N         = world_normal / n_len;
    const vec3  L         = normalize(sky.sun_direction.xyz);
    const float cos_theta = dot(N, L);

    // 背光面本来就在阴影里，偏移没有意义；同时 cos→0 时 tan 发散，必须挡掉
    if (cos_theta <= 1.0e-3)
        return world_pos;

    const float sin_theta = sqrt(max(1.0 - cos_theta * cos_theta, 0.0));
    const float offset    = min(strength * (sin_theta / cos_theta), SHADOW_NORMAL_OFFSET_MAX);

    return world_pos + N * offset;
}
#endif

float EvalCascadePCF(uint c, vec3 light_ndc, vec2 shadow_uv)
{
    const vec2  texel      = shadow.cascades[c].inv_shadow_map_size;
    const float pcf_radius = shadow.cascades[c].shadow_params.y;
    const float bias       = shadow.cascades[c].shadow_params.x;

    // 环形寻址（Toroidal Clipmap / 滚动缓存）：
    // 若 cache_offset 非零，通过 offset 偏移并求余 fract() 映射回物理纹理坐标，
    // taps 越界时环绕。cache_offset 恒为 0（滚动缓存未接线）时环绕只剩副作用：
    // 半径 ~1.5 texel 的 PCF taps 越过贴图边界后跳到对侧取深度，在每个级联
    // 方框边缘留 ~2 texel 宽的杂斑圈。故 wrap 由 cache_offset 驱动，未启用
    // 滚动缓存时越界 tap 钳在贴图边缘（与"边界外无数据=最近纹素"语义一致）。
    const bool toroidal_wrap =
           shadow.cascades[c].cache_offset.x != 0.0
        || shadow.cascades[c].cache_offset.y != 0.0;

    const vec2 offset_uv = vec2(shadow.cascades[c].cache_offset.xy) * texel;
    const vec2 phys_uv   = toroidal_wrap ? fract(shadow_uv + offset_uv)
                                         : shadow_uv + offset_uv;

    const float current_depth = light_ndc.z + bias;
    const float layer         = float(shadow.cascades[c].shadow_tex.y);
    const uint  tex_handle    = shadow.cascades[c].shadow_tex.x;

#if HGL_SHADOW_PCF_POISSON_TAPS > 0
    const float unshadowed = EvalPoissonPCF(tex_handle, ShadowPCFSampler, phys_uv, texel,
                                            pcf_radius, current_depth, toroidal_wrap,
                                            ShadowPoissonPhase(gl_FragCoord.xy));
#else
    float lit = 0.0;
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            vec2 tap = phys_uv + vec2(float(dx), float(dy)) * (texel * pcf_radius);
            tap = toroidal_wrap ? fract(tap)
                                : clamp(tap, vec2(0.0), vec2(1.0));
            lit += Sample2DArrayShadow(tex_handle, ShadowPCFSampler, tap, layer, current_depth);
        }
    }

    const float unshadowed = lit * (1.0 / 9.0);
#endif

    return mix(shadow.cascades[c].shadow_params.z, 1.0, unshadowed);
}

// 对单个级联求阴影因子。
//
// out_edge_dist 返回片元在 UV 空间到级联方框边界的距离（可为负，表示已越界）：
//   >= 0 该级联在本片元处有数据，可参与边界混合
//   < 0  该级联在本片元处**没有可用数据**（片元落在该级联的深度窗口之外 / 背向光源）。
//
// out_edge_dist 目前只服务于动态层（级联 0）的方框边界淡出。
// 它**不能**用来决定"换一级联"：级联归属必须按深度区间判定（见 EvalCascadeChain），
// 否则用一个更远的级联去顶替"本级无数据"的片元，会把远景贴图渗进近景范围。
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

// 评估指定级联区间 [first_c, last_c] 内的阴影因子。
//
// 硬规则（改动前请先读 .ai/skills/SKILL_CASCADED_SHADOW_CSM.md 第 2 节）：
//   1. 级联归属**只**由 view_depth 与 cascade_params.y(=split_far) 决定：
//      每级只负责自己的 [split_near, split_far) 区间，同级重叠时由更近的一级负责；
//   2. 被屏蔽的级联（shadow_tex.x == 0）在它自己的区间里就是"没有任何阴影数据"，
//      必须返回受光。**绝不允许下沉到更远的级联去顶替**，否则关闭 CSM 1 后
//      近景（0.1~50m）会被 CSM 2 那张粗粒度贴图接管 —— 现象正是
//      "CSM 2 的内容出现在 CSM 1 的范围"，而且因为两张贴图画的是同一批静态物件，
//      交接处看起来还是无缝的，极易误判为"映射错位"。
//   3. 被屏蔽的级联**不得影响别的级联**：本级区间内的结果与本级之后各级的开关无关。
float EvalCascadeChain(uint first_c, uint last_c, vec3 worldPos, float view_depth)
{
    uint selected = last_c;                 // 超出所有区间时由最后一级兜底
    for (uint c = first_c; c <= last_c; ++c)
    {
        if (view_depth <= shadow.cascades[c].cascade_params.y)
        {
            selected = c;
            break;
        }
    }

    // 该深度区间无阴影数据（本级被屏蔽）→ 受光，禁止下沉到更远的级联
    if (shadow.cascades[selected].shadow_tex.x == 0u)
        return 1.0;

    const float split_near = shadow.cascades[selected].cascade_params.x;
    const float split_far  = shadow.cascades[selected].cascade_params.y;
    // 交界带宽度：世界单位（米），由控制器写入 cascade_params.w；
    // 用世界单位而非比例，是为了让各级交界带的视觉宽度一致
    const float band = shadow.cascades[selected].cascade_params.w;

    float edge_dist = -1.0;
    const float shadow_factor = EvalCascadeShadowAt(selected, worldPos, edge_dist);

    // 交界带：与下一级取暗叠加(min)，本级始终保持整强度、不做淡出。
    // 这样交界处是"叠加"而不是"本级被下一级顶替"；下一级在此处无数据时返回 1.0，
    // 对 min() 天然无副作用。
    // 注意：下一级被屏蔽时直接返回本级结果，屏蔽一级不得波及相邻级。
    if (band > 0.0 && selected + 1u <= last_c
        && shadow.cascades[selected + 1u].shadow_tex.x != 0u
        && view_depth > (split_far - band))
    {
        float next_edge = -1.0;
        return min(shadow_factor, EvalCascadeShadowAt(selected + 1u, worldPos, next_edge));
    }

    // 末级：在 max_distance 附近按比例带平滑淡出到受光，避免阴影范围边缘硬切
    if (selected + 1u > last_c)
    {
        const float outer_band = shadow.cascades[selected].cascade_params.z
                               * max(split_far - split_near, 1.0e-3);
        if (outer_band > 0.0 && view_depth > (split_far - outer_band))
            return mix(1.0, shadow_factor, clamp((split_far - view_depth) / outer_band, 0.0, 1.0));
    }

    return shadow_factor;
}

float EvalPCFShadowAt(vec3 worldPos, vec3 selectPos)
{
    if (shadow.shadow_tex.x == 0u)
        return 1.0;

    // 多级级联 CSM 模式 (csm_params.x > 0)
    if (shadow.csm_params.x > 0u)
    {
        const uint cascade_count = min(shadow.csm_params.x, 4u);

        // ── 选级：按"相机 → 片元"的前向深度与每级 cascade_params.y(=split_far) 匹配 ──
        // 用 selectPos（未做算法线偏移的真实着色点）：法线偏移只是采样技巧，
        // 不该把片元推过 split 边界，否则级联接缝处会闪出错误的一级。
        const vec3  cam_fwd_raw = camera.view_line;
        const float cam_fwd_len = length(cam_fwd_raw);

        const float view_depth = (cam_fwd_len > 1.0e-6)
                               ? dot(selectPos - camera.camera_world_pos, cam_fwd_raw / cam_fwd_len)
                               : length(selectPos - camera.camera_world_pos);

        // csm_params.y == 2u: 动静分层叠加模式（CSM 0 动态层，CSM 1..N-1 静态层）
        if (shadow.csm_params.y == 2u && cascade_count > 1u)
        {
            // 1. 静态层覆盖链（从级联 1 到最后一级，全场景静态阴影）
            const float static_shadow = EvalCascadeChain(1u, cascade_count - 1u, worldPos, view_depth);

            // 2. 动态层（级联 0，仅在动态距离内求值并在远端淡出）
            float dynamic_shadow = 1.0;
            const float dyn_far = shadow.cascades[0].cascade_params.y;
            if (shadow.cascades[0].shadow_tex.x != 0u && view_depth <= dyn_far)
            {
                float dyn_edge = -1.0;
                dynamic_shadow = EvalCascadeShadowAt(0u, worldPos, dyn_edge);

                const float dyn_blend_w = shadow.cascades[0].cascade_params.z;
                if (dyn_blend_w > 0.0 && dyn_edge >= 0.0)
                {
                    const float dyn_near = shadow.cascades[0].cascade_params.x;
                    const float band = dyn_blend_w * max(dyn_far - dyn_near, 1.0e-3);
                    const float fade_depth = clamp((dyn_far - view_depth) / band, 0.0, 1.0);
                    const float fade_uv = smoothstep(0.0, dyn_blend_w, dyn_edge);
                    const float blend = min(fade_depth, fade_uv);
                    dynamic_shadow = mix(1.0, dynamic_shadow, blend);
                }
                else if (dyn_edge < 0.0)
                {
                    dynamic_shadow = 1.0;
                }
            }

            // 动静合并：取更暗的阴影因子
            return min(dynamic_shadow, static_shadow);
        }

        // 常规单链 CSM 模式
        return EvalCascadeChain(0u, cascade_count - 1u, worldPos, view_depth);
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

#if HGL_SHADOW_PCF_POISSON_TAPS > 0
    const float unshadowed = EvalPoissonPCF(tex_handle, ShadowPCFSampler, shadow_uv, texel,
                                            pcf_radius, current_depth, false,
                                            ShadowPoissonPhase(gl_FragCoord.xy));
#else
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
#endif

    return mix(shadow.shadow_params.z, 1.0, unshadowed);
}

/// 兼容入口：采样点与选级点相同。
float EvalPCFShadow(vec3 worldPos)
{
    return EvalPCFShadowAt(worldPos, worldPos);
}

float GetShadowFactor(SurfaceInput surface)
{
    vec3 sample_pos = surface.worldPos;

#if HGL_SHADOW_NORMAL_OFFSET > 0
    // shadow_params.w = 法线偏移强度（世界单位米）。控制器把同一个值写进每一级，
    // 因此读级联 0 即可；未配置时该字段是结构体默认值 0.0，等于不偏移。
    sample_pos = ShadowNormalOffsetPosition(surface.worldPos, surface.worldNormal,
                                           shadow.cascades[0].shadow_params.w);
#endif

    return EvalPCFShadowAt(sample_pos, surface.worldPos);
}

#endif // PCF_SHADOW_GLSL
