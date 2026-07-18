// textureblinnphong_surface.glsl — Texture Blinn-Phong surface function (world-space)
// Bindless 纹理采样：通过 GetTextureHandle(si.textureLayerID, slot) 查表

// MI SSBO
#include "common/material_instance_ssbo.glsl"
struct MaterialInstance
{
    float normal_strength;
};
MI_SSBO;

// Bindless 纹理
#include "common/descriptor_macros.glsl"
#include "common/instance_rows_ssbo.glsl"
TEXTURE_LAYER_ROWS_SSBO;
#include "common/bindless_textures.glsl"

// Sky light
#include "common/skylight_simple.glsl"

// ─── helpers ───

vec3 halfLambert(vec3 n, vec3 l)
{
    float NdotL = max(dot(n, l), 0.0);
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

vec3 ResolveAlbedoColor(uint iid, vec2 uv)
{
    vec4 c = SAMPLE_BINDLESS_SLOT_2D(iid, TEXTURE_SLOT_BASE_COLOR, uv);
    vec3 rgb = c.rgb;
    if (max(max(rgb.r, rgb.g), rgb.b) < 0.0001)
    {
        vec4 center = SAMPLE_BINDLESS_SLOT_2D(iid, TEXTURE_SLOT_BASE_COLOR, vec2(0.5, 0.5));
        rgb = center.rgb;
        if (max(max(rgb.r, rgb.g), rgb.b) < 0.0001)
            rgb = vec3(max(c.a, center.a));
    }
    return rgb;
}

vec3 ResolveSurfaceNormal(uint iid, vec3 input_normal, vec2 uv, float normal_strength)
{
    vec3 sampled_normal = SAMPLE_BINDLESS_SLOT_2D(iid, TEXTURE_SLOT_NORMAL, uv).xyz * 2.0 - 1.0;
    sampled_normal.y = -sampled_normal.y;
    return normalize(input_normal + vec3(sampled_normal.xy, 0.0) * normal_strength);
}

float ResolveSurfaceRoughness(uint iid, float base_roughness, vec2 uv)
{
    float roughness_tex = SAMPLE_BINDLESS_SLOT_2D(iid, TEXTURE_SLOT_ROUGHNESS, uv).r;
    return clamp(base_roughness * roughness_tex, 0.04, 1.0);
}

// ─── surface entry ───

SurfaceOutput EvalSurface(SurfaceInput si, uint miID)
{
    MaterialInstance mi = mtl.mi[miID];
    const uint iid = si.textureLayerID;

    const float spec_strength = 0.6;
    const float F0            = 0.04;

    vec2 uv = ResolveSurfaceUV(si.uv0);
    vec3 base_color = ResolveAlbedoColor(iid, uv);

    float ns = mi.normal_strength > 0.0001 ? mi.normal_strength : 0.35;
    float roughness  = ResolveSurfaceRoughness(iid, 0.8, uv);
    float spec_power = mix(96.0, 8.0, roughness);

    vec3 N = ResolveSurfaceNormal(iid, si.worldNormal, uv, ns);
    vec3 V = si.viewDir;
    vec3 L = normalize(ULRE_GetSkyLightDir());
    vec3 H = normalize(L + V);

    vec3 diffuse = base_color * halfLambert(N, L);

    float NdotH = max(dot(N, H), 0.0);
    float spec  = pow(NdotH, spec_power) * spec_strength;
    float F     = fresnelSchlick(max(dot(V, H), 0.0), F0);

    vec3 sunColor   = max(ULRE_GetSkyLightColor(), vec3(0.20));
    vec3 skyAmbient = ULRE_GetSkyAmbientColor();

    vec3 color = diffuse + spec * F;
    color *= sunColor;
    color += skyAmbient * 0.25;

    SurfaceOutput so;
    so.baseColor = color;
    so.normal    = N;
    so.metallic  = 0.0;
    so.roughness = roughness;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    so.alpha     = 1.0;
    return so;
}


