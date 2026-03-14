// Standard Surface Function — 基础 PBR 材质
// 第一版：最简 PBR 采样逻辑

#include "common/surface_interface.glsl"

// 材质纹理 — binding 由 Compositor / Material 系统最终确定
#if QUALITY_TIER >= 1
layout(set=2, binding=1) uniform sampler2D TexAlbedo;
#endif
#if QUALITY_TIER >= 2
layout(set=2, binding=2) uniform sampler2D TexNormal;
layout(set=2, binding=3) uniform sampler2D TexMR;      // metallic(G) + roughness(B)
#endif

// 材质实例数据 — 从 MI SSBO 读取
struct MI_Standard
{
    vec4  base_color_factor;
    float metallic_factor;
    float roughness_factor;
    float ao_strength;
    float emissive_strength;
    // ...
};

SurfaceOutput EvalSurface(SurfaceInput si, MI_Standard mi)
{
    SurfaceOutput so;
    so.baseColor = mi.base_color_factor.rgb;
    so.alpha     = mi.base_color_factor.a;
    so.metallic  = mi.metallic_factor;
    so.roughness = mi.roughness_factor;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    so.normal    = si.worldNormal;

#if QUALITY_TIER >= 1
    // 采样 Albedo 纹理
    so.baseColor *= texture(TexAlbedo, si.uv0).rgb;
#endif

#if QUALITY_TIER >= 2
    // 采样法线贴图
    // so.normal = ...
    // 采样 MR 贴图
    // vec2 mr = texture(TexMR, si.uv0).gb;
    // so.metallic *= mr.r;
    // so.roughness *= mr.g;
#endif

    return so;
}

float EvalAlpha(SurfaceInput si, MI_Standard mi)
{
    return mi.base_color_factor.a;
}
