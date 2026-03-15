// Unlit Luminance Surface Function — 顶点亮度 × MI 颜色
// MI_Luminance: vec4 Color (16 bytes)
// baseColor = MI.Color.rgb × luminance，alpha = MI.Color.a

#include "common/surface_interface.glsl"

struct MI_Luminance
{
    vec4 color;
};

// MI SSBO
layout(set=MATERIAL_SET, binding=0) readonly buffer MaterialInstanceData { MI_Luminance mi_data[]; } mtl;

SurfaceOutput EvalSurface(SurfaceInput si, uint materialInstanceID)
{
    MI_Luminance mi = mtl.mi_data[materialInstanceID];

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
    return mtl.mi_data[materialInstanceID].color.a;
}
