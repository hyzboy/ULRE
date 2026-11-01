// @ulre begin
// @ulre name texture_projection_octahedral
// @ulre kind Shared
// @ulre priority 0
// @ulre sampler_reference TrilinearClamp
// @ulre uses bindless_textures
// @ulre uses texture_projection_equirect
// @ulre end
// 材质源投影实现 — 八面体展开图（`*.TexOct`）。
//
// 与 2D 版本同名同签名、共享同一个 include guard：由 frag_options 的那行
// `#include` 选中本文件后，2D 默认实现就被 guard 挡住，主着色器无需 #if。
//
// 逆映射必须与 TexConv 的正投影逐项对齐（src/Tools/TexConv/core/src/
// cube_pipeline.cpp 的 ProjectFloatCubeToOctahedral）：
//   * 输出是边长 = 源面宽 ×2 的正方形，texel (px,py) 对应
//     x = 2(px+0.5)/size-1，y = 2(py+0.5)/size-1 —— 即图像第 0 行是 y = -1，
//     与 GLSL 的 v = 0 落在首行一致，故这里**不做 Y 翻转**；
//   * 正投影对每个 texel 取 z = 1-|x|-|y|，z<0 时按 (1-|y|,1-|x|)·sign 折叠，
//     再按最大分量选面 ⇒ 其逆过程是标准八面体编码（L1 归一化 + z<0 折叠）。
// 采样使用 TrilinearClampSampler 与基于 3D 向量局部图谱的显式梯度（textureGrad）：
// 八面体展开在南半球四角存在象限跳变，硬件 2x2 差分在跨折叠经线缝时产生剧烈
// 导数跳变引发 Mip 爆炸（呈现虚线痕迹）；通过在当前片元局部连续图谱内评估
// 差分，消除导数阶跃；同时 ClampToEdge 避免东西对极纹素边界污染。
#ifndef MATERIAL_TEXTURE_PROJECTION_GLSL
#define MATERIAL_TEXTURE_PROJECTION_GLSL

#include "module/provider/material_source/texture_projection/texture_projection_equirect.glsl"

// 在 baseDir 的局部八面体图谱下评估 direction 的八面体展开坐标（[-1,1]²）。
// 锁定 baseDir 的 Z<0 折叠符号，确保 2x2 像素块在跨越经线缝时导数连续平滑。
vec2 MaterialOctahedralEncodeDirectionLocal(vec3 direction, vec3 baseDir)
{
    const float l1Norm = abs(direction.x) + abs(direction.y) + abs(direction.z);
    vec2 octahedral = direction.xy / max(l1Norm, 1e-8);
    if (baseDir.z < 0.0)
    {
        const vec2 foldSign = vec2(baseDir.x < 0.0 ? -1.0 : 1.0,
                                   baseDir.y < 0.0 ? -1.0 : 1.0);
        octahedral = (1.0 - abs(octahedral.yx)) * foldSign;
    }
    return octahedral;
}

vec2 MaterialOctahedralEncodeDirection(vec3 direction)
{
    return MaterialOctahedralEncodeDirectionLocal(direction, direction);
}

vec4 SampleMaterialProjected(uvec2 texture_reference, vec2 equirectUv)
{
    const vec3 direction = MaterialEquirectUvToDirection(equirectUv);
    const vec3 ddir_dx = dFdx(direction);
    const vec3 ddir_dy = dFdy(direction);

    const vec2 oct_base = MaterialOctahedralEncodeDirection(direction);
    const vec2 oct_dx = MaterialOctahedralEncodeDirectionLocal(direction + ddir_dx, direction);
    const vec2 oct_dy = MaterialOctahedralEncodeDirectionLocal(direction + ddir_dy, direction);

    const vec2 octahedralUv = oct_base * 0.5 + 0.5;
    const vec2 dPdx = (oct_dx - oct_base) * 0.5;
    const vec2 dPdy = (oct_dy - oct_base) * 0.5;

    return Sample2DArrayGrad(texture_reference.x,
                             TrilinearClampSampler,
                             octahedralUv,
                             float(texture_reference.y),
                             dPdx,
                             dPdy);
}

#endif // MATERIAL_TEXTURE_PROJECTION_GLSL

