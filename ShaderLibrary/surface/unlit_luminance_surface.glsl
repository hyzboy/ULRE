// Unlit Luminance Surface Function — 顶点亮度 × MI 颜色
// MI_Luminance: vec4 Color (16 bytes)
// baseColor = MI.Color.rgb × luminance，alpha = MI.Color.a

#include "common/surface_interface.glsl"

SurfaceOutput EvalSurface(SurfaceInput si, uint materialInstanceID)
{
    EmissiveSurfaceData mi = mtl.mi[materialInstanceID];

    SurfaceOutput so;
    so.baseColor = si.luminance * mi.color.rgb;
    so.alpha     = mi.color.a;
    so.normal    = si.worldNormal;
    so.metallic  = 0.0;
    so.roughness = 1.0;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    return so;
}

float EvalAlpha(SurfaceInput si, uint materialInstanceID)
{
    return mtl.mi[materialInstanceID].color.a;
}
