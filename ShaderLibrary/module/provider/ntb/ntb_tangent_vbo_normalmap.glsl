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

#include "module/interface/ntb_interface.glsl"
#include "module/provider/ntb/ntb_orthonormal.glsl"
#include "module/descriptor/bindless_textures.glsl"

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

    // ── Christian Schüler 余切标架 ─────────────────────────────────
    // Vulkan 屏幕空间原点在左上（Y 向下增加），与 OpenGL（Y 向上）相反，
    // 导致 dFdy 符号相反。若直接套用原始公式会导致 T 与 B 均反向（凹凸反向）。
    // 将 dp2 与 duv2 取反对齐右手屏幕标架推导约定。
    const vec3 dp1 = dFdx(si.worldPos);
    const vec3 dp2 = -dFdy(si.worldPos);
    const vec2 duv1 = dFdx(si.uv0);
    const vec2 duv2 = -dFdy(si.uv0);

    const vec3 dp2perp = cross(dp2, ntb.N);
    const vec3 dp1perp = cross(ntb.N, dp1);
    const vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    const vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    const float max_len_sq = max(dot(T, T), dot(B, B));
    if (max_len_sq < 1e-24)
    {
        return BuildOrthoNTB(ntb.N);
    }

    const float invmax = inversesqrt(max_len_sq);
    ntb.T = T * invmax;
    ntb.B = B * invmax;

    const vec4 normal_sample =
        Sample2DArray(
            normalTexHandle,
            TrilinearAnisoSampler,
            si.uv0,
            float(normalTexture.y));

    vec3 nm = normal_sample.xyz * 2.0 - 1.0;
    nm.y = -nm.y; // GLSL/Vulkan Green Channel 翻转 (glTF OpenGL 规范: +Y 对应 -V)

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

    ntb.N = normalize(mat3(ntb.T, ntb.B, ntb.N) * tangentNormal);
    return ntb;
}

#endif // NTB_TANGENT_VBO_NORMALMAP_GLSL
