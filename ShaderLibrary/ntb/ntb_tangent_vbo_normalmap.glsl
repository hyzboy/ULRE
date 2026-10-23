// @ulre begin
// @ulre name ntb_tangent_vbo_normalmap
// @ulre kind Utility
// @ulre priority 0
// @ulre slot ntb_provider
// @ulre uses ntb_interface
// @ulre uses ntb_orthonormal
// @ulre uses bindless_textures
// @ulre texture_reference normal Fragment optional fallback
// @ulre sampler_reference Trilinear
// @ulre end
// NTB Normal Map — 余切标架（cotangent frame）法线贴图
//
// 命名说明：本模块原名 "tangent_vbo"，原意是「切线取自顶点流 Tangent 属性」。
// 引擎当前不向 Fragment 阶段传 Tangent/Binormal varying（InterStageSemantic 里
// 登记了 WorldTangent/WorldBinormal，但 mesh varying 发射白名单刻意不含它们），
// 所以顶点流切线这条路并不存在。旧实现因此退化成 BuildOrthoNTB()：用世界轴
// 造任意正交基 —— T/B 与 UV 毫无关系，贴图的「上」不对应纹理的 +V，法线方向
// 随物体朝向乱翻。模块名保留只为不破坏既有材质引用，实现已改为导数标架。
//
// 现实现取 Lengyel 的 cotangent frame：由世界位置的屏幕空间导数与 UV 导数还原
// 与纹理严格对齐的切线基。没有顶点切线流也能得到正确的切线空间，代价是每像素
// 多 4 条导数指令，且要求同一 quad 内导数有定义（fragment 阶段天然满足）。
#ifndef NTB_TANGENT_VBO_NORMALMAP_GLSL
#define NTB_TANGENT_VBO_NORMALMAP_GLSL

#include "common/ntb_interface.glsl"
#include "ntb/ntb_orthonormal.glsl"
#include "common/bindless_textures.glsl"

NTBSpace GetNTB(NTBInput ntb_input)
{
    const SurfaceInput si = ntb_input.surface;
    NTBSpace ntb;
    ntb.N = normalize(si.worldNormal);

    const uvec2 normalTexture =
        MTL_TEX(ntb_input.dataIndex).tex_normal;
    const uint normalTexHandle = normalTexture.x;
    if (normalTexHandle == 0u)
    {
        // 没有法线贴图：只需一个任意正交基当作「未扰动」的标架。
        return BuildOrthoNTB(ntb.N);
    }

    // ── 余切标架：T 沿 +U、B 沿 +V，与贴图切线空间严格对齐 ────────────
    const vec3 pos_dx = dFdx(si.worldPos);
    const vec3 pos_dy = dFdy(si.worldPos);
    const vec2 tex_dx = dFdx(si.uv0);
    const vec2 tex_dy = dFdy(si.uv0);

    vec3 T = pos_dx * tex_dy.y - pos_dy * tex_dx.y;
    T -= ntb.N * dot(ntb.N, T); // Gram-Schmidt：抹掉法线方向上的分量

    if (dot(T, T) < 1e-12)
    {
        // UV 退化（零面积三角形 / 常量 UV / 贴图被压成一条线）：导数为零，
        // normalize 会产出 NaN 并顺着光照链路污染整帧。退回任意正交基。
        return BuildOrthoNTB(ntb.N);
    }

    ntb.T = normalize(T);
    ntb.B = cross(ntb.N, ntb.T); // 右手系，且已单位化

    const vec4 normal_sample =
        Sample2DArray(
            normalTexHandle,
            TrilinearSampler,
            si.uv0,
            float(normalTexture.y));

    vec3 nm = normal_sample.xyz * 2.0 - 1.0;
    nm.y = -nm.y; // GLSL/Vulkan Green Channel 翻转

#if defined(MTL_TEX_NORMAL_CHANNELS) && (MTL_TEX_NORMAL_CHANNELS == 2)
    // 双通道法线(BC5)：只存 XY，Z 用球面公式还原（与 UE/Unity 的 BC5 法线一致）。
    // 宏由材质声明 channels = 2 时由 ShaderGen 注入，见 FragmentTemplateComposer。
    const vec3 tangentNormal =
        normalize(vec3(nm.xy * ntb_input.normalScale,
                       sqrt(max(0.0, 1.0 - dot(nm.xy, nm.xy)))));
#else
    const vec3 tangentNormal =
        normalize(vec3(nm.xy * ntb_input.normalScale, nm.z));
#endif

    // 只替换法线；T/B 保持 UV 对齐的几何标架（旧实现此处用世界轴重建 T/B，
    // 等于把切线空间刚退掉，故不再重正交化）。
    ntb.N = normalize(mat3(ntb.T, ntb.B, ntb.N) * tangentNormal);
    return ntb;
}

#endif // NTB_TANGENT_VBO_NORMALMAP_GLSL
