// Unlit Color3D Surface Function — 最简纯色材质
// 不使用任何纹理，不参与光照计算
// MI_Unlit: 仅包含 vec4 color (16 bytes)

#include "common/surface_interface.glsl"
// 材质实例数据 — 仅一个颜色值
struct EmissiveSurfaceData
{
    vec4 color;     // RGBA
};

layout(set=MI_SET, binding=MI_BINDING) readonly buffer EmissiveSurfaceBuffer {
    EmissiveSurfaceData mi[];
} mtl;

SurfaceOutput EvalSurface(SurfaceInput si, uint materialInstanceID)
{
    EmissiveSurfaceData mi = mtl.mi[materialInstanceID];

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

float EvalAlpha(SurfaceInput si, uint materialInstanceID)
{
    return mtl.mi[materialInstanceID].color.a;
}
