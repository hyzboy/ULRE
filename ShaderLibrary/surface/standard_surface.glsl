// standard_surface.glsl — Standard Lit Surface (flattened, no quality branches)
// Always evaluates a single PBR-style lighting path.
// Texture source mode variants are preserved via *_TEX_ARRAY_MODE macros.

#include "common/surface_interface.glsl"

// ─── MI SSBO ─────────────────────────────────────────────────────────────────
#include "common/material_instance_ssbo.glsl"
struct MaterialInstance
{
    uint  base_color;    // packed RGBA8_UNORM, read with unpackUnorm4x8()
    float metallic;
    float roughness;
    float normal_scale;  // normal map intensity
};
MI_SSBO;

// ─── Sky Light ────────────────────────────────────────────────────────────────
#include "common/skylight_simple.glsl"

// ─── Helpers ─────────────────────────────────────────────────────────────────

float D_GGX(float NdotH, float alpha2)
{
    float d = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    return alpha2 / (3.14159265 * d * d + 1e-7);
}

float G_Smith(float NdotV, float NdotL, float roughness)
{
    float k  = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float gv = NdotV / (NdotV * (1.0 - k) + k + 1e-7);
    float gl = NdotL / (NdotL * (1.0 - k) + k + 1e-7);
    return gv * gl;
}

vec3 F_Schlick(float VdotH, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

// ─── Surface Entry ────────────────────────────────────────────────────────────

SurfaceOutput EvalSurface(SurfaceInput si, uint miID)
{
    MaterialInstance mi = mtl.mi[miID];

    vec3 N = normalize(si.worldNormal);
    vec3 V = si.viewDir;
    vec3 L = normalize(ULRE_GetSkyLightDir());

    vec3 sunColor   = max(ULRE_GetSkyLightColor(), vec3(0.2));
    vec3 skyAmbient = ULRE_GetSkyAmbientColor();

    // ── Base color ────────────────────────────────────────────────────────────
    vec3 albedo = unpackUnorm4x8(mi.base_color).rgb;
    albedo *= GetSamplerBaseColor(si.uv0).rgb;

    float metallic  = clamp(mi.metallic,  0.0, 1.0);
    float roughness = clamp(mi.roughness, 0.04, 1.0);

    // ── Normal Map ────────────────────────────────────────────────────────────
    vec3 nm = GetSamplerNormal(si.uv0).xyz * 2.0 - 1.0;
    nm.y = -nm.y;
    N = normalize(N + vec3(nm.xy, 0.0) * mi.normal_scale);

    // ── MR Map ────────────────────────────────────────────────────────────────
    vec2 mr    = GetSamplerRoughness(si.uv0).rg;
    metallic   = clamp(metallic  * mr.r, 0.0, 1.0);
    roughness  = clamp(roughness * mr.g, 0.04, 1.0);

    // ── Simplified Cook-Torrance PBR (no IBL, no cubemap) ────────────────────
    float NdotL  = max(dot(N, L), 0.0);
    float NdotV  = max(dot(N, V), 1e-4);
    vec3  H      = normalize(V + L);
    float NdotH  = max(dot(N, H), 0.0);
    float VdotH  = max(dot(V, H), 0.0);

    float alpha2 = roughness * roughness * roughness * roughness;
    float D      = D_GGX(NdotH, alpha2);
    float G      = G_Smith(NdotV, NdotL, roughness);
    vec3  F0     = mix(vec3(0.04), albedo, metallic);
    vec3  F      = F_Schlick(VdotH, F0);

    vec3 kd       = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse  = kd * albedo / 3.14159265 * NdotL;
    vec3 specular = D * G * F / max(4.0 * NdotV * NdotL, 1e-4) * NdotL;

    vec3 color  = (diffuse + specular) * sunColor;
    color      += skyAmbient * albedo * (1.0 - metallic) * 0.2;

    SurfaceOutput so;
    so.baseColor = color;
    so.normal    = N;
    so.metallic  = metallic;
    so.roughness = roughness;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    so.alpha     = 1.0;
    return so;
}

float EvalAlpha(SurfaceInput si, uint miID)
{
    return 1.0;
}
