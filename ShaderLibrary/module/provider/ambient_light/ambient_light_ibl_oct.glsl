// @ulre begin
// @ulre name ambient_light_ibl_oct
// @ulre kind Utility
// @ulre priority 0
// @ulre slot ambient_light_provider
// @ulre provides_capability ambient_diffuse|ambient_specular
// @ulre uses lighting_interface
// @ulre uses scene_ubo
// @ulre uses bindless_textures
// @ulre sampler_reference TrilinearClamp
// @ulre sampler_reference Linear
// @ulre end
// Indirect Lighting — IBL(Image Based Lighting)split-sum 近似,环境贴图为**八面体展开**版本。
//
// 与 `ambient_light_ibl`(Cubemap 版)逐项同构:漫反射仍按世界法线取照射度、镜面仍按反射
// 向量取预滤波图 + BRDF LUT split-sum 缩放、粗糙度仍按 `lod = roughness*(级数-1)` 取级、
// LDR 曝光调制完全同一套(见下方 ldr_param 段)。**唯一差别是纹理寻址方式**:
//
//   Cubemap 版  : 方向 → SampleCubeArray / SampleCubeArrayLod(硬件立方体寻址)
//   Octahedral 版: 方向 → 八面体逆映射得 uv → Sample2DArrayLod(普通 2D 寻址)
//
// 三个 env_tex 句柄的槽位含义不变(x=irradiance, y=prefilter, z=BRDF LUT, w=prefilter mip 数),
// 只是 x/y 指向 TexConv `/octa /ibl` 产出的 `.TexOct`(2D 容器,单层,带 mip 链)而非 `.TexCube`。
// 环境层用 `EnvironmentSystem::SetAmbientProvider("ambient_light_ibl_oct")` 与之配对选择,
// 因此调用方(Lit 类材质)完全无感知——光照模型与使用处一行都不用改。
//
// 逆映射必须与 TexConv 正投影逐项对齐(src/Tools/TexConv/core/src/cube_pipeline.cpp 的
// ProjectFloatCubeToOctahedral):正投影对每个 texel 取
//   x = 2(px+0.5)/size-1, y = 2(py+0.5)/size-1, z = 1-|x|-|y|
//   z<0 时按 (1-|y|,1-|x|)·sign 折叠,再按最大分量选面
// 所以图像的 L1 归一化坐标即 uv,且**不做 Y 翻转**(图像第 0 行是 y=-1,与 GLSL v=0 同向)。
//
// 采样用 TrilinearClampSampler + 显式 LOD:八面体展开在南半球四角存在象限跳变,
// 隐式梯度在跨折叠经线时会因导数阶跃引发 mip 爆炸(虚线痕迹),显式 LOD 绕开该问题;
// ClampToEdge 避免展开图外边界纹素污染。
#ifndef INDIRECT_IBL_OCT_GLSL
#define INDIRECT_IBL_OCT_GLSL

#include "module/interface/lighting_interface.glsl"
#include "module/descriptor/ubo/scene_ubo.glsl"
#include "module/descriptor/bindless_textures.glsl"

// 单位方向 → 八面体展开 uv([-1,1]²)。与 TexConv 正投影互逆(L1 归一化 + z<0 折叠)。
vec2 IBL_OctahedralEncodeDirection(vec3 direction)
{
    const float l1_norm = abs(direction.x) + abs(direction.y) + abs(direction.z);
    vec2 octahedral = direction.xy / max(l1_norm, 1e-8);

    if (direction.z < 0.0)
    {
        const vec2 fold_sign = vec2(direction.x < 0.0 ? -1.0 : 1.0,
                                    direction.y < 0.0 ? -1.0 : 1.0);
        octahedral = (1.0 - abs(octahedral.yx)) * fold_sign;
    }

    return octahedral;
}

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

    // 漫反射项:八面体 irradiance × 反照率 × 非金属系数
    // 单张八面体图只有第 0 级有意义(照射度本就低频),固定 LOD 0 采样。
    const vec2 irr_uv = IBL_OctahedralEncodeDirection(N) * 0.5 + 0.5;
    const vec3 irradiance =
        Sample2DArrayLod(ambient_env.env_tex.x, TrilinearClampSampler, irr_uv, 0.0, 0.0).rgb;

    const vec3 diffuse =
        irradiance * lighting.baseColor * (1.0 - lighting.metallic);

    // 镜面项:八面体 prefiltered × (F0·scale + bias)(BRDF LUT split-sum)
    const vec2 env_brdf =
        Sample2D(ambient_env.env_tex.z, LinearSampler, vec2(NdotV, lighting.roughness)).rg;

    // 粗糙度 → mip LOD:第 i 级烘的是 roughness = i/(级数-1)，与烘焙端同一映射。
    // w=0/1 表示只有第 0 级(固定粗糙度)，此时 max(...,1u)-1 = 0，lod 恒 0。
    const float prefilter_last_lod = float(max(ambient_env.env_tex.w, 1u) - 1u);
    const float prefilter_lod =
        clamp(lighting.roughness, 0.0, 1.0) * prefilter_last_lod;

    const vec2 pre_uv = IBL_OctahedralEncodeDirection(R) * 0.5 + 0.5;
    const vec3 prefiltered =
        Sample2DArrayLod(ambient_env.env_tex.y, TrilinearClampSampler, pre_uv, 0.0, prefilter_lod).rgb;

    const vec3 specular = prefiltered * (F0 * env_brdf.x + env_brdf.y);

    vec3 ambient = (diffuse + specular) * lighting.ao;

    // LDR 曝光度与太阳光强度动态调制（仅在 ldr_param.x > 0 时对 LDR 生效）
    // 与 ambient_light_ibl 完全同一语义:八面体与立方体只是寻址方式不同,曝光必须一致。
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

#endif // INDIRECT_IBL_OCT_GLSL
