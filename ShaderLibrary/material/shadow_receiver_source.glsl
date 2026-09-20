// @ulre begin
// @ulre name shadow_receiver_source
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
// @ulre require Resource MaterialData
// @ulre require ProducedSemantic UV0
// @ulre texture_reference base_color Fragment optional fallback
// @ulre texture_reference shadow_map Fragment optional fallback
// @ulre uses material_source_interface
// @ulre uses bindless_textures
// @ulre end
// 阴影接收材质源 —— example/Basic/ShadowMap.cpp 的地面（receiver）专用。
//
// 与 material/pbr_surface_source.glsl 的唯一区别：贴图颜色写进 baseColor 之后，
// 再乘一个由 shadow map 求出的"遮挡遮罩"，于是地面就显示出环上网格投下的影子。
// 其余 PBR 参数（metallic/roughness/normalScale…）原样透出，走既有的 forward_lit
// 合成器，光照模型完全不变。
//
// ## 为什么可以直接拿 uv0 当光源空间 uv
//
// 光源相机被摆成"正俯视"（见 ShadowMap.cpp::CreateLightCamera），且 fov 取
//     2 * atan(地面半宽 / 相机高度)
// 于是：
//   * 光锥在地面处的截面恰好等于地面本身 → 地面在 shadow map 里铺满 [0,1]²；
//   * 地面垂直于光轴 → 到光源的视线方向距离 lin 处处相等；
//   * "世界 XY → 光源空间 uv"因此是纯线性映射，而 PlaneSquare 的 uv0 本身
//     就是 0.5 + 世界XY/20，两者只差一个 Y 镜像（见下）。
// 这也是"只改 ShaderLibrary/示例层"能达到的效果上限：地面必须是平的、
// 且光源必须接近正上方。真正的通用做法要在引擎侧补光照空间 UBO + shadow
// provider + 模板接线，见 example/Basic/ShadowMap.cpp 文件头的 TODO。
//
// ## Y 镜像的来历
//
// 本引擎投影矩阵 m[1][1] = -f（Vulkan NDC 的 +Y 朝下，见 ReversedZProj.cpp），
// 经 viewport（正向高度）变换后
//     uv.y  = 0.5 - world.y / 20
// 而 PlaneSquare 的
//     uv0.y = 0.5 + world.y / 20
// 两者互为镜像，所以采样前要把 v 翻回来。
//
// ## 采样器必须用 ShadowMapSampler（Nearest）
//
// shadow map 是 PF_D32F = VK_FORMAT_D32_SFLOAT，纯深度格式不支持线性过滤
// （没有 VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT）。用 Trilinear/UI
// 这类 Linear 采样器读深度图属于未定义行为，AMD 上恒返回 0 —— 于是
// "裸地基准值 == 采样值"永远成立，地面一片漆黑且完全没有影子。
// 定义见 ShaderLibrary/sampler.toml 的 ShadowMap 项（索引 7）。

#ifndef SHADOW_RECEIVER_SOURCE_GLSL
#define SHADOW_RECEIVER_SOURCE_GLSL

#include "common/material_source_interface.glsl"
#include "common/bindless_textures.glsl"

// ── 与 ShadowMap.cpp 对齐的常量 ────────────────────────────────────────

/// shadow map 边长（kShadowMapSize）
const float SHADOW_RECEIVER_MAP_SIZE = 1024.0;
const float SHADOW_RECEIVER_TEXEL    = 1.0 / SHADOW_RECEIVER_MAP_SIZE;

/// PCF 采样半径（单位：texel）
const float SHADOW_RECEIVER_PCF_RADIUS = 1.5;

/// 深度差阈值。深度值域是 0~1，遮挡物与裸地的差值量级在 0.05~0.8，
/// 阈值只用来滤掉浮点噪声，取 0.002 足够小又不至于抖动。
const float SHADOW_RECEIVER_BIAS = 0.002;

/// 被遮挡时的反照率系数（0 = 全黑，1 = 不受影响）
const float SHADOW_RECEIVER_DARKNESS = 0.12;

/// 参考基准 texel 的 uv：地面四角之一（世界 x≈-9.4 / y≈+9.4），
/// 离环上网格（半径 6.5 + 网格自身半径 ≤ 2.1）最远，恒为"未被遮挡"。
///
/// 用"当前采样值 − 基准值"的绝对差判遮挡，好处是不必在 shader 里硬编码
/// 光源的 near / 高度 / 投影公式，也不必关心深度图清屏值是 0.0（地面不写
/// 进深度图：裸地 = 清屏值）还是地面自身的深度（地面也写进深度图）——
/// 两种情况基准都取在当地，判定结果一致。
const vec2 SHADOW_RECEIVER_REF_UV = vec2(0.031, 0.031);

/// 地面 uv0 → 光源空间 uv（只差一个 Y 镜像，见文件头）
vec2 ShadowReceiverMapUV(vec2 ground_uv)
{
    return vec2(ground_uv.x, 1.0 - ground_uv.y);
}

/// 3×3 PCF 求遮挡遮罩
/// @return 1.0 = 完全受光，0.0 = 完全被遮挡
///
/// 注意采样器必须是 ShadowMapSampler（Nearest + ClampToEdge）：
/// 本引擎的 shadow map 是 PF_D32F（= VK_FORMAT_D32_SFLOAT），该格式**不支持**
/// VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT，用 Linear 采样器读它是
/// 未定义行为（AMD 实测恒返回 0）。一旦读回 0，"裸地基准值 == 采样值"就恒成立，
/// 遮挡判定永不触发，地面表现为"完全没有影子"。PCF 的柔化由这里的 9 次
/// Nearest 取样自己完成，不需要硬件线性过滤。
float EvalShadowReceiverMask(uint material_data_index, vec2 uv)
{
    const uvec2 shadow_map = MTL_TEX(material_data_index).tex_shadow_map;
    if (shadow_map.x == 0u)
        return 1.0;             // 未绑定 shadow map：按完全受光处理

    const float layer = float(shadow_map.y);
    const float bare  = Sample2DArray(shadow_map.x, ShadowMapSampler, SHADOW_RECEIVER_REF_UV, layer).r;

    float occluded = 0.0;

    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            const vec2  tap = uv + vec2(float(dx), float(dy))
                                   * (SHADOW_RECEIVER_TEXEL * SHADOW_RECEIVER_PCF_RADIUS);
            const float d   = Sample2DArray(shadow_map.x, ShadowMapSampler, tap, layer).r;

            occluded += (abs(d - bare) > SHADOW_RECEIVER_BIAS) ? 1.0 : 0.0;
        }
    }

    return 1.0 - occluded * (1.0 / 9.0);
}

MaterialSourceOutput EvalMaterialSource(MaterialSourceInput source_input)
{
    PBRSurfaceRow material_data = MTL_ROW(source_input.dataIndex);

    MaterialSourceOutput material_output;
    material_output.baseColor   = material_data.base_color.rgb;
    material_output.metallic    = clamp(material_data.metallic, 0.0, 1.0);
    material_output.roughness   = clamp(material_data.roughness, 0.04, 1.0);
    material_output.fresnel     = clamp(material_data.fresnel, 0.0, 1.0);
    material_output.normalScale = material_data.normal_scale;
    material_output.ao          = 1.0;
    material_output.emissive    = vec3(0.0);
    material_output.alpha       = 1.0;

    const uint material_data_index = source_input.dataIndex;
    const vec2 material_uv         = source_input.surface.uv0;

    material_output.baseColor *=
        SampleOptional(MTL_TEX(material_data_index).tex_base_color, TrilinearSampler, material_uv, vec4(1.0)).rgb;

    // ── 阴影：投影到光源空间，与 shadow map 的深度差做比较 ──
    const float shadow = EvalShadowReceiverMask(material_data_index, ShadowReceiverMapUV(material_uv));

    material_output.baseColor *= mix(vec3(SHADOW_RECEIVER_DARKNESS), vec3(1.0), shadow);

    return material_output;
}

float EvalMaterialAlpha(MaterialSourceInput source_input)
{
    // 地面不透明（与 pbr_surface_source 的 alpha 语义一致）
    return 1.0;
}

#endif // SHADOW_RECEIVER_SOURCE_GLSL
