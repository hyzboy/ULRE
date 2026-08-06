// @ulre begin
// @ulre name ntb_derivative_normalmap
// @ulre kind Utility
// @ulre priority 0
// @ulre uses ntb_interface
// @ulre uses bindless_textures
// @ulre end
// NTB Derivative Normal Map — 基于 dFdx / dFdy 屏幕空间偏导推导切线空间并解算法线贴图
#ifndef NTB_DERIVATIVE_NORMALMAP_GLSL
#define NTB_DERIVATIVE_NORMALMAP_GLSL

#include "common/ntb_interface.glsl"
#include "common/bindless_textures.glsl"

NTBSpace EvalNTBSpace(SurfaceInput si, uint dataIndex, float normalScale, uint normalTexHandle)
{
    NTBSpace ntb;
    ntb.N = normalize(si.worldNormal);

    if (normalTexHandle != 0u)
    {
        vec3 pos_dx = dFdx(si.worldPos);
        vec3 pos_dy = dFdy(si.worldPos);
        vec2 tex_dx = dFdx(si.uv0);
        vec2 tex_dy = dFdy(si.uv0);

        vec3 N = ntb.N;
        vec3 T = (pos_dx * tex_dy.y - pos_dy * tex_dx.y);
        T = normalize(T - N * dot(N, T));
        vec3 B = cross(N, T);

        vec3 nm = SampleBindless2D(normalTexHandle, si.uv0).xyz * 2.0 - 1.0;
        nm.y = -nm.y;
        vec3 tangentNormal = normalize(vec3(nm.xy * normalScale, nm.z));

        mat3 TBN = mat3(T, B, N);
        ntb.N = normalize(TBN * tangentNormal);
        ntb.T = T;
        ntb.B = B;
    }
    else
    {
        ntb = BuildOrthoNTB(ntb.N);
    }

    return ntb;
}

#endif // NTB_DERIVATIVE_NORMALMAP_GLSL
