// @ulre begin
// @ulre name unlit_color3d_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre require ProducedSemantic WorldNormal
// @ulre require Resource MaterialData
// @ulre uses surface_interface
// @ulre end
// Unlit Color3D Surface Function — 最简纯色材质
// 不使用任何纹理，不参与光照计算
// MI_Unlit: 仅包含 vec4 color (16 bytes)

#include "common/surface_interface.glsl"

SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    EmissiveSurfaceData mi = mtl.data[dataIndex];

    SurfaceOutput so;
    so.baseColor = mi.color.rgb;
    so.alpha     = mi.color.a;
    so.normal    = si.worldNormal;
    so.metallic  = 0.0;
    so.roughness = 1.0;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    return so;
}

float EvalAlpha(SurfaceInput si, uint dataIndex)
{
    return mtl.data[dataIndex].color.a;
}
