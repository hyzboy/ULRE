// @ulre begin
// @ulre name shadow_receiver_source
// @ulre kind Utility
// @ulre priority 0
// @ulre slot material_source_provider
// @ulre require Resource MaterialData
// @ulre require Resource SkyLight
// @ulre require ProducedSemantic UV0
// @ulre texture_reference base_color Fragment optional fallback
// @ulre texture_reference shadow_map Fragment optional fallback
// @ulre uses material_source_interface
// @ulre uses bindless_textures
// @ulre uses scene_ubo
// @ulre end
// 阴影接收材质源 —— example/Basic/ShadowMap.cpp 的地面（receiver）专用。
//
// 与 material/pbr_surface_source.glsl 的唯一区别：贴图颜色写进 baseColor 之后，
// 再乘一个由 shadow map 求出的"遮挡遮罩"，于是地面就显示出环上网格投下的影子。
// 其余 PBR 参数（metallic/roughness/normalScale…）原样透出，走既有的 forward_lit
// 合成器，光照模型完全不变。
//
// ## 光源是倾斜 + 环绕的，uv0 不再等于光源空间 uv
//
// 早期版本把光源相机摆成"正俯视"，于是地面在 shadow map 里铺满 [0,1]²、
// uv0 恰好可以当光源空间 uv 用。**一旦把光源倾斜（为了把影子拉长），
// 这个前提就不再成立**：地面不再垂直于光轴，光锥在地面处的截面也不再等于地面。
//
// 现在的做法是：在 shader 里用 **sky.sun_direction** 把光源相机整个重建出来，
// 再把地面点解析投影到光源空间，求出 uv 与自身深度。
//
//   * 光源相机恒为 ViewModel 摆法：位于 +sun_direction * DISTANCE，target = 原点。
//     于是 forward = -sun_direction、pos = sun_direction * DISTANCE，
//     右/上向量用与 C++ CameraSystem::ComputeRightUp 完全相同的式子算出；
//   * sun_direction 是 SkyInfo UBO 的字段（Scene 集 binding 1），示例每帧写入，
//     并且**直接取自光源相机自身的 forward**（取反），所以两侧不可能对不上；
//   * 地面点的世界 XY 由 uv0 反推（PlaneSquare 的 uv0 = 0.5 + 世界XY / 20）。
//
// ## 深度比较（reversed-Z）
//
// 本引擎投影走 MakeInfiniteReversedZProj，NDC 深度 = near / lin
// （lin = 沿光轴的视线距离；近平面 → 1.0，无穷远 → 0.0），采样器读回的就是它。
// 地面**不写进** shadow map（只有环上网格是"投射者"），所以未被遮挡的纹素恒为
// 清屏值 0.0f。于是遮挡判据就是
//     d > 地面自身深度 + bias
// （reversed-Z 下"更靠近光源"对应深度更大）。地面自身的深度由同一个投影解析算出，
// 不再需要早期版本那种"取一个已知裸地 texel 当基准"的技巧。
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
// sky.sun_direction —— 光源方向的唯一来源。
// forward_lit.glsl.tmpl 已经 include 过它，这里再写一次是幂等的（有 include guard），
// 但能让本模块单独可读、不依赖被拼进来的顺序。
#include "ubo/scene_ubo.glsl"

// ── 与 example/Basic/ShadowMap.cpp 对齐的光源相机参数 ──────────────────
// 改这里任何一个都必须同步改 ShadowMap.cpp 里同名的 kLight* / kReceiver* 常量。

/// 光源相机到原点的距离（ShadowMap.cpp: kLightDistance）
const float SHADOW_RECEIVER_LIGHT_DISTANCE = 36.0;

/// 光源相机近平面（ShadowMap.cpp: kLightNear）
const float SHADOW_RECEIVER_LIGHT_NEAR = 18.0;

/// tan(光源相机 fov / 2)，fov = 50°（ShadowMap.cpp: kLightFov）
const float SHADOW_RECEIVER_LIGHT_TAN_HALF_FOV = 0.46630766;

/// 世界"上"方向 —— 光源相机的滚转基准，必须与 C++ 侧 CameraComponent 的
/// world_up 默认值 (0,0,1) 一致，否则 right/up 会差一个滚转角。
const vec3 SHADOW_RECEIVER_WORLD_UP = vec3(0.0, 0.0, 1.0);

/// PlaneSquare 的 uv0 从 0→1 所对应的世界跨度（ShadowMap.cpp: kReceiverPlaneScale）
const float SHADOW_RECEIVER_GROUND_WORLD_SIZE = 20.0;

/// shadow map 边长（ShadowMap.cpp: kShadowMapSize）
const float SHADOW_RECEIVER_MAP_SIZE = 1024.0;
const float SHADOW_RECEIVER_TEXEL    = 1.0 / SHADOW_RECEIVER_MAP_SIZE;

/// PCF 采样半径（单位：texel）
const float SHADOW_RECEIVER_PCF_RADIUS = 1.5;

/// 深度差阈值。用来吸收浮点误差、以及"shader 里重建的相机"与"C++ 里真正的
/// 相机"之间极微小的不一致（两者同源，理论上只剩 float 精度量级）。
/// 取太大会出现"影子与物体脱开"（peter-panning），所以给得很小。
const float SHADOW_RECEIVER_BIAS = 0.002;

/// 被遮挡时的反照率系数（0 = 全黑，1 = 不受影响）
const float SHADOW_RECEIVER_DARKNESS = 0.12;

/// 由 sun_direction 重建出来的光源相机。
struct ShadowReceiverLight
{
    vec3 pos;       ///< 相机位置 = sun_direction * DISTANCE
    vec3 right;     ///< = normalize(cross(forward, world_up))
    vec3 up;        ///< = normalize(cross(right, forward))
    vec3 forward;   ///< = -sun_direction
};

/// 重建光源相机。与 ShadowMap.cpp::CreateLightCamera 的摆法一一对应：
///   forward = -sun_direction
///   position = target - forward * distance （target = 原点）
/// 右/上向量与 CameraSystem::ComputeRightUp 同构。
///
/// 注意不能用"光源接近正上方"的构型：那时 forward 与 world_up 平行，
/// cross 退化成 0，right/up 全是 NaN（C++ 侧的 ComputeRightUp 同样有这个坑，
/// 所以示例把光源固定在 28° 仰角）。
ShadowReceiverLight BuildShadowReceiverLight()
{
    ShadowReceiverLight l;

    const vec3 to_sun = normalize(sky.sun_direction.xyz);

    l.forward = -to_sun;
    l.pos     = to_sun * SHADOW_RECEIVER_LIGHT_DISTANCE;
    l.right   = normalize(cross(l.forward, SHADOW_RECEIVER_WORLD_UP));
    l.up      = normalize(cross(l.right, l.forward));

    return l;
}

/// 地面点 → shadow map 采样 uv + 该点自身的光源空间深度。
///
/// @param ground_uv    地面 uv0（PlaneSquare 上 = 0.5 + 世界XY / 20）
/// @param out_map_uv   输出：shadow map 采样 uv
/// @param out_depth    输出：地面点自身的光源空间深度（reversed-Z：near / lin）
/// @return false 表示该点落在光源视锥之外，调用方应按"不受遮挡"处理
bool ShadowReceiverProjectGround(vec2 ground_uv, out vec2 out_map_uv, out float out_depth)
{
    const ShadowReceiverLight light = BuildShadowReceiverLight();

    // uv0 → 世界 XY（地面在 z = 0 平面上）
    const vec3 ground = vec3((ground_uv.x - 0.5) * SHADOW_RECEIVER_GROUND_WORLD_SIZE,
                             (ground_uv.y - 0.5) * SHADOW_RECEIVER_GROUND_WORLD_SIZE,
                             0.0);

    const vec3  rel = ground - light.pos;
    const float lin = dot(rel, light.forward);
    if (lin <= SHADOW_RECEIVER_LIGHT_NEAR)
        return false;

    // 与 MakeInfiniteReversedZProj 同构：m[0][0] = f/aspect（shadow map 是正方形，
    // aspect = 1）、m[1][1] = -f（本引擎 Vulkan NDC 的 +Y 朝下）。
    // uv 这里**不需要**再翻一次 Y —— 镜像已经包含在那个负号里。早期"uv0 直接当
    // 光源 uv"的版本因为绕过了投影，才要手动补一次 Y 翻转。
    const float f   = 1.0 / SHADOW_RECEIVER_LIGHT_TAN_HALF_FOV;
    const vec2  ndc = vec2( f * dot(rel, light.right) / lin,
                           -f * dot(rel, light.up)    / lin);

    out_map_uv = vec2(0.5) + 0.5 * ndc;
    out_depth  = SHADOW_RECEIVER_LIGHT_NEAR / lin;

    // 视锥外（含 PCF 半径与一个 texel 的余量）一律按不受遮挡处理，
    // 否则 ClampToEdge 会把边缘纹素抹开、在贴图边界上拉出一条假影。
    const float margin = SHADOW_RECEIVER_TEXEL * (SHADOW_RECEIVER_PCF_RADIUS + 1.0);

    return all(greaterThanEqual(out_map_uv, vec2(margin)))
        && all(lessThanEqual(out_map_uv, vec2(1.0 - margin)));
}

/// 3×3 PCF 求遮挡遮罩
/// @return 1.0 = 完全受光，0.0 = 完全被遮挡
///
/// 采样器必须是 ShadowMapSampler（Nearest + ClampToEdge）：
/// 本引擎的 shadow map 是 PF_D32F（= VK_FORMAT_D32_SFLOAT），该格式**不支持**
/// VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT，用 Linear 采样器读它是
/// 未定义行为（AMD 实测恒返回 0）。一旦读回 0，遮挡判定永不触发，
/// 地面表现为"完全没有影子"。PCF 的柔化由这里的 9 次 Nearest 取样自己完成，
/// 不需要硬件线性过滤。
float EvalShadowReceiverMask(uint material_data_index, vec2 ground_uv)
{
    const uvec2 shadow_map = MTL_TEX(material_data_index).tex_shadow_map;
    if (shadow_map.x == 0u)
        return 1.0;             // 未绑定 shadow map：按完全受光处理

    vec2  map_uv;
    float ground_depth;
    if (!ShadowReceiverProjectGround(ground_uv, map_uv, ground_depth))
        return 1.0;

    const float layer = float(shadow_map.y);

    float occluded = 0.0;

    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            const vec2  tap = map_uv + vec2(float(dx), float(dy))
                                   * (SHADOW_RECEIVER_TEXEL * SHADOW_RECEIVER_PCF_RADIUS);
            const float d   = Sample2DArray(shadow_map.x, ShadowMapSampler, tap, layer).r;

            // 地面不写进 shadow map，未被遮挡的纹素是清屏值 0.0（reversed-Z 的远平面）；
            // 只有比地面更靠近光源的投射者才会给出更大的深度值。
            occluded += (d > ground_depth + SHADOW_RECEIVER_BIAS) ? 1.0 : 0.0;
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

    // ── 阴影：把地面点投影到光源空间，与 shadow map 的深度做比较 ──
    // 注意传的是地面 uv0（不是光源 uv）：投影在 EvalShadowReceiverMask 里完成。
    const float shadow = EvalShadowReceiverMask(material_data_index, material_uv);

    material_output.baseColor *= mix(vec3(SHADOW_RECEIVER_DARKNESS), vec3(1.0), shadow);

    return material_output;
}

float EvalMaterialAlpha(MaterialSourceInput source_input)
{
    // 地面不透明（与 pbr_surface_source 的 alpha 语义一致）
    return 1.0;
}

#endif // SHADOW_RECEIVER_SOURCE_GLSL
