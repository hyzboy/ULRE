
struct MaterialInstance
{
    uint  base_color;           float metallic;
    float roughness;
    float fresnel;
    float ibl_intensity;
    float normal_strength;
};

#include "common/ssbo_material_instance.glsl"
#include "common/skylight_simple.glsl"


vec3 halfLambert(vec3 normal, vec3 lightDir)
{
    float NdotL = max(dot(normal, lightDir), 0.0);
    return vec3(NdotL * 0.5 + 0.5);
}

float fresnelSchlick(float cosTheta, float F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

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

vec3 ResolveSurfaceNormal(vec3 input_normal, vec2 uv, float normal_strength)
{
    vec3 sampled_normal = GetSamplerNormal(uv).xyz * 2.0 - 1.0;
    sampled_normal.y = -sampled_normal.y;
    return normalize(input_normal + vec3(sampled_normal.xy, 0.0) * normal_strength);
}

float ResolveSurfaceRoughness(float base_roughness, vec2 uv)
{
    float roughness_tex = GetSamplerRoughness(uv).r;
    return clamp(base_roughness * roughness_tex, 0.04, 1.0);
}


SurfaceOutput EvalSurface(SurfaceInput si)
{
    MaterialInstance mi = GetMaterialInstance();

    vec2 uv = ResolveSurfaceUV(si.uv0);
    float ns = mi.normal_strength > 0.0001 ? mi.normal_strength : 0.35;
    vec3 N = ResolveSurfaceNormal(si.worldNormal, uv, ns);
    vec3 V = si.viewDir;
    vec3 L = normalize(ULRE_GetSkyLightDir());

    vec3 sampled_albedo = ResolveAlbedoColor(uv);
    vec4 base_color = unpackUnorm4x8(mi.base_color) * vec4(sampled_albedo, 1.0);

    float roughness_mix = ResolveSurfaceRoughness(mi.roughness, uv);

    vec3 diffuse = base_color.rgb * halfLambert(N, L);
    diffuse = max(diffuse, vec3(0.1));

    vec3 H = normalize(L + V);
    float spec_power = mix(96.0, 8.0, roughness_mix);
    float spec = pow(max(dot(N, H), 0.0), spec_power) * mi.metallic;
    float fres = fresnelSchlick(max(dot(V, H), 0.0), mi.fresnel);

    vec3 sunColor   = max(ULRE_GetSkyLightColor(), vec3(0.20));
    vec3 skyAmbient = ULRE_GetSkyAmbientColor();

    vec3 color = diffuse + spec * fres;
    color *= sunColor;
    color += skyAmbient * 0.25;

    SurfaceOutput so;
    so.baseColor = color;
    so.normal    = N;
    so.metallic  = mi.metallic;
    so.roughness = roughness_mix;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    so.alpha     = 1.0;
    return so;
}
