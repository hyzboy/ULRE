// @ulre begin
// @ulre name unlit_luminance_surface
// @ulre kind Surface
// @ulre priority 0
// @ulre require ProducedSemantic WorldNormal
// @ulre require ProducedSemantic Luminance
// @ulre uses surface_interface
// @ulre uses unlit_source
// @ulre end
// Unlit Luminance Surface Function — 顶点亮度 × MI 颜色
// 通过 EvalUnlitSource() provider 获取 SSBO 颜色数据
// baseColor = SSBO.Color.rgb × luminance，alpha = SSBO.Color.a

#include "common/surface_interface.glsl"
#include "material/unlit_source.glsl"

#ifndef HGL_COVERAGE_ONLY
SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    EmissiveSurfaceData material_data = EvalUnlitSource(dataIndex);

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
#endif

float EvalAlpha(SurfaceInput si, uint dataIndex)
{
    return EvalUnlitSource(dataIndex).color.a;
}
