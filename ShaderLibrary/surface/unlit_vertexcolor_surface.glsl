// @ulre begin
// @ulre name unlit_vertexcolor_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre require ProducedSemantic WorldNormal
// @ulre require ProducedSemantic Color
// @ulre uses surface_interface
// @ulre end
// Unlit VertexColor Surface Function — 顶点色直通
// 无 MI，baseColor 直接取自 SurfaceInput.vertexColor

#include "common/surface_interface.glsl"

SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    SurfaceOutput so;
    so.baseColor = si.vertexColor.rgb;
    so.alpha     = si.vertexColor.a;
    so.normal    = si.worldNormal;
    so.metallic  = 0.0;
    so.roughness = 1.0;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    return so;
}

float EvalAlpha(SurfaceInput si, uint dataIndex)
{
    return si.vertexColor.a;
}
