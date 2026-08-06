// @ulre begin
// @ulre name ntb_tangent_vbo_normalmap
// @ulre kind Utility
// @ulre priority 0
// @ulre uses ntb_interface
// @ulre uses bindless_textures
// @ulre end
// NTB Tangent / Normal Map — 结合法线贴图与 TBN 空间转换
#ifndef NTB_TANGENT_VBO_NORMALMAP_GLSL
#define NTB_TANGENT_VBO_NORMALMAP_GLSL

#include "common/ntb_interface.glsl"
#include "common/bindless_textures.glsl"

NTBSpace EvalNTBSpace(SurfaceInput si, uint dataIndex, float normalScale, uint normalTexHandle)
{
    NTBSpace ntb = BuildOrthoNTB(si.worldNormal);

    if (normalTexHandle != 0u)
    {
        vec3 nm = SampleBindless2D(normalTexHandle, si.uv0).xyz * 2.0 - 1.0;
        nm.y = -nm.y; // GLSL/Vulkan Green Channel 翻转
        vec3 tangentNormal = normalize(vec3(nm.xy * normalScale, nm.z));

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
