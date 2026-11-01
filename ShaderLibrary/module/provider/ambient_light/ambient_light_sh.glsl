// @ulre begin
// @ulre name ambient_light_sh
// @ulre kind Utility
// @ulre priority 0
// @ulre slot ambient_light_provider
// @ulre provides_capability ambient_diffuse
// @ulre uses lighting_interface
// @ulre uses scene_ubo
// @ulre end
// Indirect Lighting — L2 球谐（SH9）天光。
// 与 ambient_light_ibl / ambient_light_ibl_oct 是**同一槽位的第三种算法**：只读
// ambient_env 参数块的 sh[9]（9 个 vec4，系数由 CPU 逐帧合成），不读任何贴图 ⇒
// 无需纹理、无需烘焙、无需 bindless 句柄，代价是只有低频漫反射（无镜面项）。
//
// 漫反射辐照度用 Ramamoorthi & Hanrahan 的解析卷积（"An Efficient Representation
// for Irradiance Environment Maps"）：把入射辐亮度投影到前 9 个 SH 基函数后，余弦
// 核的球面积分对每个基函数只是一个常数缩放 ⇒ 逐顶点/逐像素只需 9 次乘加。
//
//   基函数（Z-up，本引擎世界坐标约定；与 AmbientEnvInfo::sh 的下标一一对应）
//     idx0  Y00  ∝ 1                 idx3  Y11  ∝ x          idx6  Y20  ∝ 3z²-1
//     idx1  Y1-1 ∝ y                 idx4  Y2-2 ∝ x*y        idx7  Y21  ∝ x*z
//     idx2  Y10  ∝ z                 idx5  Y2-1 ∝ y*z        idx8  Y22  ∝ x²-y²
//   卷积常数（convolve(cos) 的 A_l / (2l+1)·… 归一化后的系数）
//     l=0: 0.886227   l=1: 1.023327   l=2 (xy,yz,xz): 0.858086
//     l=2 (3z²-1): 0.247708   l=2 (x²-y²): 0.429043
//
// 公式在全 0 系数下返回 0（= 无天光，物体仅剩直接光），与旧实现未布探针时一致。
#ifndef INDIRECT_SH_AMBIENT_GLSL
#define INDIRECT_SH_AMBIENT_GLSL

#include "module/interface/lighting_interface.glsl"
#include "module/descriptor/ubo/scene_ubo.glsl"

// 逐方向求 SH9 辐照度
vec3 EvalSHAnalyticIrradiance(
    const vec3 n
) {
    const vec3 n2 = n * n;

    const vec3 c0 = ambient_env.sh[0].xyz;
    const vec3 c1 = ambient_env.sh[1].xyz;
    const vec3 c2 = ambient_env.sh[2].xyz;
    const vec3 c3 = ambient_env.sh[3].xyz;
    const vec3 c4 = ambient_env.sh[4].xyz;
    const vec3 c5 = ambient_env.sh[5].xyz;
    const vec3 c6 = ambient_env.sh[6].xyz;
    const vec3 c7 = ambient_env.sh[7].xyz;
    const vec3 c8 = ambient_env.sh[8].xyz;

    return  c0               * 0.886227
          + c1 * n.y         * 1.023327
          + c2 * n.z         * 1.023327
          + c3 * n.x         * 1.023327
          + c4 * (n.x * n.y) * 0.858086
          + c5 * (n.y * n.z) * 0.858086
          + c6 * (3.0 * n2.z - 1.0) * 0.247708
          + c7 * (n.x * n.z) * 0.858086
          + c8 * (n2.x - n2.y) * 0.429043;
}

vec3 EvalIndirectLighting(
    LightingInput lighting
) {
    // 法线必须归一化：SH 求值对方向长度非线性（n.x*n.y 等项会随长度缩放）
    const vec3 irradiance = EvalSHAnalyticIrradiance(normalize(lighting.normal));

    // 与 IBL 版漫反射项同构：辐照度 × 反照率 × 非金属系数 × AO。
    // 不含镜面项（SH9 无方向分辨力，给出的是余弦卷积后的漫反射辐照度）。
    // 也不吃 ldr_param 曝光调制：SH 系数在 CPU 侧已按当前太阳/大气色合成，
    // 再乘一次曝光会双重计权。
    return irradiance * lighting.baseColor * (1.0 - lighting.metallic) * lighting.ao;
}

#endif // INDIRECT_SH_AMBIENT_GLSL
