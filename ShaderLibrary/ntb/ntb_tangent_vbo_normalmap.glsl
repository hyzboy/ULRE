// @ulre begin
// @ulre name ntb_tangent_vbo_normalmap
// @ulre kind Utility
// @ulre priority 0
// @ulre slot ntb_provider
// @ulre uses ntb_interface
// @ulre uses bindless_textures
// @ulre texture_reference normal Fragment optional fallback
// @ulre end
// NTB Tangent / Normal Map — 结合法线贴图与 TBN 空间转换
#ifndef NTB_TANGENT_VBO_NORMALMAP_GLSL
#define NTB_TANGENT_VBO_NORMALMAP_GLSL

#include "common/ntb_interface.glsl"
#include "ntb/ntb_orthonormal.glsl"
#include "common/bindless_textures.glsl"

NTBSpace GetNTB(NTBInput ntb_input)
{
    const SurfaceInput si = ntb_input.surface;
    NTBSpace ntb = BuildOrthoNTB(si.worldNormal);
    const uvec2 normalTexture =
        MTL_TEX(ntb_input.dataIndex).tex_normal;
    const uint normalTexHandle = normalTexture.x;

    if (normalTexHandle != 0u)
    {
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

        mat3 TBN = mat3(ntb.T, ntb.B, ntb.N);
        ntb.N = normalize(TBN * tangentNormal);
        // 重新正交化 T 与 B
        vec3 up = abs(ntb.N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
        ntb.T = normalize(cross(up, ntb.N));
        ntb.B = cross(ntb.N, ntb.T);
    }

    return ntb;
}

#endif // NTB_TANGENT_VBO_NORMALMAP_GLSL
