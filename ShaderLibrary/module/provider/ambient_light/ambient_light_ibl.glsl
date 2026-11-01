// @ulre begin
// @ulre name ambient_light_ibl
// @ulre kind Utility
// @ulre priority 0
// @ulre slot ambient_light_provider
// @ulre provides_capability ambient_diffuse|ambient_specular
// @ulre uses lighting_interface
// @ulre uses scene_ubo
// @ulre uses bindless_textures
// @ulre sampler_reference Trilinear
// @ulre sampler_reference Linear
// @ulre end
// Indirect Lighting — IBL(Image Based Lighting)split-sum 近似。
// 替换 indirect_sky_ambient(程序化天空 ambient):
//   漫反射:irradiance cubemap 按世界法线采样(预卷积余弦半球)
//   镜面:prefiltered cubemap 按反射向量采样 + BRDF LUT 缩放
// 三张环境纹理经 ambient_env 参数块(ambient_light_provider 槽,世界表 addr_ambient_env)
// 的 env_tex 句柄下发(0=未绑定→黑色,
// 此时物体仅剩直接光,行为与未加载 IBL 探针一致)。
// prefilter cubemap 带完整 mip 链:第 i 级 = 粗糙度 i/(级数-1) 的 GGX 预卷积
// (TexConv 侧 PrefilterRoughness 定同一映射),故按 lod = roughness*(级数-1) 采样,
// 级数由 env_tex.w 下发(0/1 → 只有一级,退化为固定粗糙度,与旧行为等价)。
#ifndef INDIRECT_IBL_GLSL
#define INDIRECT_IBL_GLSL

#include "module/interface/lighting_interface.glsl"
#include "module/descriptor/ubo/scene_ubo.glsl"
#include "module/descriptor/bindless_textures.glsl"

vec3 EvalIndirectLighting(
    LightingInput lighting
) {
    if(ambient_env.env_tex.x == 0u && ambient_env.env_tex.y == 0u)
        return vec3(0.0);

    const vec3 N = normalize(lighting.normal);
    const vec3 V = lighting.viewDir;
    const vec3 R = reflect(-V, N);

    const float NdotV = max(dot(N, V), 0.0);

    const vec3 F0 = mix(vec3(lighting.fresnel), lighting.baseColor, lighting.metallic);

    // 漫反射项:irradiance cubemap × 反照率 × 非金属系数
    const vec3 irradiance =
        SampleCubeArray(ambient_env.env_tex.x, TrilinearSampler, vec4(N, 0.0)).rgb;

    const vec3 diffuse =
        irradiance * lighting.baseColor * (1.0 - lighting.metallic);

    // 镜面项:prefiltered cubemap × (F0·scale + bias)(BRDF LUT split-sum)
    const vec2 env_brdf =
        Sample2D(ambient_env.env_tex.z, LinearSampler, vec2(NdotV, lighting.roughness)).rg;

    // 粗糙度 → mip LOD:第 i 级烘的是 roughness = i/(级数-1)，与烘焙端同一映射。
    // w=0/1 表示只有第 0 级(固定粗糙度)，此时 max(...,1u)-1 = 0，lod 恒 0。
    const float prefilter_last_lod = float(max(ambient_env.env_tex.w, 1u) - 1u);
    const float prefilter_lod =
        clamp(lighting.roughness, 0.0, 1.0) * prefilter_last_lod;

    const vec3 prefiltered =
        SampleCubeArrayLod(ambient_env.env_tex.y, TrilinearSampler, R, prefilter_lod).rgb;

    const vec3 specular = prefiltered * (F0 * env_brdf.x + env_brdf.y);

    vec3 ambient = (diffuse + specular) * lighting.ao;

    // LDR 曝光度与太阳光强度动态调制（仅在 ldr_param.x > 0 时对 LDR 生效）
    //   ldr_param.x = is_ldr 开关（>0.0 启用）
    //   ldr_param.y = exposure_base 基准曝光倍率
    //   ldr_param.z = sun_scale 太阳高度角与光强调制权重
    //   ldr_param.w = min_ambient 最低环境光保底比例
    if (ambient_env.ldr_param.x > 0.0)
    {
        const float base_exposure = max(ambient_env.ldr_param.y, 0.0);
        const float sun_scale = ambient_env.ldr_param.z;
        const float min_ambient = clamp(ambient_env.ldr_param.w, 0.0, 1.0);

        // 主方向光数据取自光源行 0（main_light）
        // 世界坐标系约定为 Z-up：sun_dir.z 即为太阳仰角正弦值（天顶 1.0，地平线 0.0，黑夜 < 0.0）
        const float dir_len = length(main_light.direction_intensity.xyz);
        const float sun_elevation = (dir_len > 1e-4)
            ? clamp(main_light.direction_intensity.z / dir_len, 0.0, 1.0)
            : 0.0;
        const float sun_intensity = max(main_light.direction_intensity.w, 0.0);

        const float sun_factor = sun_elevation * sun_intensity;
        const float exposure = base_exposure * (min_ambient + sun_factor * sun_scale);

        ambient *= exposure;
    }

    return ambient;
}

#endif // INDIRECT_IBL_GLSL
