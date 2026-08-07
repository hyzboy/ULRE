// @ulre begin
// @ulre name unlit_luminance_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre require ProducedSemantic WorldNormal
// @ulre require ProducedSemantic Luminance
// @ulre uses surface_interface
// @ulre end
// Unlit Luminance Surface Function — 顶点亮度 × MI 颜色
// MI_Luminance: vec4 Color (16 bytes)
// baseColor = MI.Color.rgb × luminance，alpha = MI.Color.a

#include "common/surface_interface.glsl"

SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    EmissiveSurfaceData material_data = MTL_DATA.data[dataIndex];

    SurfaceOutput so;
    so.baseColor = si.luminance * material_data.color.rgb;
    so.alpha     = material_data.color.a;
    so.normal    = si.worldNormal;
    so.metallic  = 0.0;
    so.roughness = 1.0;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    return so;
}

float EvalAlpha(SurfaceInput si, uint dataIndex)
{
    return MTL_DATA.data[dataIndex].color.a;
}
