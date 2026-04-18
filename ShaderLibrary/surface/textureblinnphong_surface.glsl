
#include "common/schema/schema_standard_params.glsl"

#include "common/ssbo_material_instance.glsl"

vec2 ResolveSurfaceUV(vec2 uv)
{
    if (abs(uv.x) + abs(uv.y) < 0.0001)
        return vec2(0.5, 0.5);
    return fract(abs(uv));
}

vec3 ResolveAlbedoColor(vec2 uv)
{
    vec4 c = GetSamplerBaseColor(uv);
    vec3 rgb = c.rgb;
    if (max(max(rgb.r, rgb.g), rgb.b) < 0.0001)
    {
        vec4 center = GetSamplerBaseColor(vec2(0.5, 0.5));
        rgb = center.rgb;
        if (max(max(rgb.r, rgb.g), rgb.b) < 0.0001)
            rgb = vec3(max(c.a, center.a));
    }
    return rgb;
}

vec3 ResolveSurfaceNormal(vec3 input_normal, vec2 uv, float normal_scale)
{
    vec3 sampled_normal = GetSamplerNormal(uv).xyz * 2.0 - 1.0;
    sampled_normal.y = -sampled_normal.y;
    return normalize(input_normal + vec3(sampled_normal.xy, 0.0) * normal_scale);
}


SurfaceOutput EvalSurface(SurfaceInput si)
{
    MaterialBindingInstance mi = GetMaterialBindingInstance();

    vec2 uv = ResolveSurfaceUV(si.uv0);
    vec3 albedo = unpackUnorm4x8(mi.base_color).rgb;
    albedo *= ResolveAlbedoColor(uv);

    float metallic = clamp(mi.metallic, 0.0, 1.0);
    float roughness = clamp(mi.roughness, 0.04, 1.0);
    float normal_scale = mi.normal_scale > 0.0001 ? mi.normal_scale : 0.35;
    vec3 N = ResolveSurfaceNormal(si.worldNormal, uv, normal_scale);

    SurfaceOutput so;
    so.baseColor = albedo;
    so.normal    = N;
    so.metallic  = metallic;
    so.roughness = roughness;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    so.alpha     = 1.0;
    return so;
}
