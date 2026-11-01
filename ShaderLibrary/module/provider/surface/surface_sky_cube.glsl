// @ulre begin
// @ulre name surface_sky_cube
// @ulre kind Surface
// @ulre priority 0
// @ulre slot surface_provider
// @ulre require ProducedSemantic WorldPosition
// @ulre texture_reference sky_cube Fragment required
// @ulre sampler_reference Trilinear
// @ulre uses surface_interface
// @ulre uses bindless_textures
// @ulre uses texture_projection_2d
// @ulre end
// === Surface Function: SkyCube ===
// 天空域的**贴图形态**表面：把 fragDirection 当方向去采样 sky_cube 槽。
//
// 槽名里的 "cube" 是**槽位的历史名字**，不是它唯一能装的格式。这里调的是
// SampleMaterialProjectedDirection — 与无光单图材质源完全同一个投影机制的另一
// 形态入口：谁实现它由 generated/frag_options.glsl 末尾那行 #include 决定，判定
// 依据是配方里 sky_cube 那条绑定的资源后缀（.TexCube → cube、.TexOct → 八面体
// 展开图、其余 → 2D 展开图）。因此**换天空贴图格式不需要换配方领域、不需要换表面
// 模块、也不需要第二份主着色器**，本文件里没有任何投影相关的 #if 分支。
//
// 为什么走"方向"版而不是 uv 版：Sky 域的输入本来就是方向（main() 把 fragDirection
// 写进 si.worldPos），几何也给不出有意义的 uv0。方向版让 cube 直接 SampleCubeArray、
// 八面体直接编码 —— 没有 方向→uv→方向 的往返量化，且八面体路径拿到的是**逐片元
// 连续方向**（早期 SkyCubeSphereOct 接缝问题正是从几何 uv 反算方向造成的）。
//
// 2D 展开图（未加 .TexCube / .TexOct 标记）按等距柱状（经纬）展开图采样，即该槽的
// 2D 语义 = equirect。方位投影（Lambert azimuthal）那类展开图吃的是**几何 uv**，
// 请继续用 Unlit 几何路径（SkyLambertDome.Unlit）。
#ifndef SURFACE_SKY_CUBE_GLSL
#define SURFACE_SKY_CUBE_GLSL

#include "module/descriptor/bindless_textures.glsl"
#include "module/provider/material_source/texture_projection/texture_projection_2d.glsl"

SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    const uvec2 tex_ref  = MTL_TEX(dataIndex).tex_sky_cube;
    const vec3  view_dir = normalize(si.worldPos);

    const vec4 color = SampleMaterialProjectedDirection(tex_ref, view_dir);

    SurfaceOutput so;
    so.baseColor = color.rgb;
    so.alpha     = 1.0;
    return so;
}
#endif // SURFACE_SKY_CUBE_GLSL