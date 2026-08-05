// @ulre begin
// @ulre name debug_normal_color
// @ulre kind Surface
// @ulre priority 0
// @ulre require ProducedSemantic WorldNormal
// @ulre require Resource MaterialData
// @ulre uses surface_interface
// @ulre end
// DebugNormalColor Surface Function
// MI_Gizmo: vec4 Color (16 bytes)
// 自带简易 Blinn-Phong 光照（硬编码太阳方向）

#include "common/surface_interface.glsl"

SurfaceOutput EvalSurface(SurfaceInput si, uint materialInstanceID)
{
    EmissiveSurfaceData mi = mtl.mi[materialInstanceID];

    const vec3 SUN_DIRECTION = normalize(vec3(0.655386, 0.491539, 0.573462));
    const vec3 SUN_COLOR     = vec3(1.0, 1.0, 1.0);

    // Half-Lambert diffuse
    float intensity = 0.5 * max(dot(si.worldNormal, SUN_DIRECTION), 0.0) + 0.5;
    vec3 direct_color = intensity * SUN_COLOR * mi.color.rgb;

    // Blinn-Phong specular
    vec3 spec_color = vec3(0.0);
    if (intensity > 0.0)
    {
        vec3 half_vector = normalize(SUN_DIRECTION + si.viewDir);
        float specular = max(dot(half_vector, si.worldNormal), 0.0);
        spec_color = specular * pow(specular, 16.0) * SUN_COLOR;
    }

    SurfaceOutput so;
    so.baseColor = direct_color + spec_color;
    so.alpha     = 1.0;
    so.normal    = si.worldNormal;
    so.metallic  = 0.0;
    so.roughness = 1.0;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    return so;
}

float EvalAlpha(SurfaceInput si, uint materialInstanceID)
{
    return 1.0;
}
