// @ulre begin
// @ulre name indirect_ibl
// @ulre kind Utility
// @ulre priority 0
// @ulre slot ambient_light_provider
// @ulre provides_capability ambient_diffuse|ambient_specular
// @ulre uses lighting_interface
// @ulre uses scene_ubo
// @ulre uses bindless_textures
// @ulre end
// Indirect Lighting — IBL(Image Based Lighting)split-sum 近似。
// 替换 indirect_sky_ambient(程序化天空 ambient):
//   漫反射:irradiance cubemap 按世界法线采样(预卷积余弦半球)
//   镜面:prefiltered cubemap 按反射向量采样 + BRDF LUT 缩放
// 三张环境纹理经 SkyInfo UBO 的 env_tex 句柄下发(场景全局,0=未绑定→黑色,
// 此时物体仅剩直接光,行为与未加载 IBL 探针一致)。
// 注:当前 prefilter cubemap 为单一粗糙度(0.5)烘焙,粗糙度变化仅通过
//     BRDF LUT 的缩放体现,不做 mip 链模糊过渡。
#ifndef INDIRECT_IBL_GLSL
#define INDIRECT_IBL_GLSL

#include "common/lighting_interface.glsl"
#include "ubo/scene_ubo.glsl"
#include "common/bindless_textures.glsl"

vec3 EvalIndirectLighting(
    LightingInput lighting
) {
    if(sky.env_tex.x == 0u && sky.env_tex.y == 0u)
        return vec3(0.0);

    const vec3 N = normalize(lighting.normal);
    const vec3 V = lighting.viewDir;
    const vec3 R = reflect(-V, N);

    const float NdotV = max(dot(N, V), 0.0);

    const vec3 F0 = mix(vec3(lighting.fresnel), lighting.baseColor, lighting.metallic);

    // 漫反射项:irradiance cubemap × 反照率 × 非金属系数
    const vec3 irradiance =
        SampleCubeArray(sky.env_tex.x, TrilinearSampler, vec4(N, 0.0)).rgb;

    const vec3 diffuse =
        irradiance * lighting.baseColor * (1.0 - lighting.metallic);

    // 镜面项:prefiltered cubemap × (F0·scale + bias)(BRDF LUT split-sum)
    const vec2 env_brdf =
        Sample2D(sky.env_tex.z, LinearSampler, vec2(NdotV, lighting.roughness)).rg;

    const vec3 prefiltered =
        SampleCubeArray(sky.env_tex.y, TrilinearSampler, vec4(R, 0.0)).rgb;

    const vec3 specular = prefiltered * (F0 * env_brdf.x + env_brdf.y);

    return (diffuse + specular) * lighting.ao;
}

#endif // INDIRECT_IBL_GLSL
