// pbrcolor3d_surface.glsl — PBR Color3D surface function (world-space, Cook-Torrance)
// Pure-color variant: no texture sampling.
// Material set bindings: mtl=0

// MI SSBO
#include "common/material_instance_ssbo.glsl"
struct MaterialInstance
{
    uint  base_color;    // packed RGBA8_UNORM
    float metallic;      // [0, 1]
    float roughness;     // [0.04, 1]
};
MI_SSBO;

// Sky light
#include "common/skylight_simple.glsl"

// ─── PBR helpers ───

const float PBR_PI = 3.14159265359;

float PBR_NDF(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PBR_PI * d * d, 0.0001);
}

float PBR_G1(float NdotX, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / max(NdotX * (1.0 - k) + k, 0.0001);
}

float PBR_G(float NdotV, float NdotL, float roughness)
{
    return PBR_G1(NdotV, roughness) * PBR_G1(NdotL, roughness);
}

vec3 PBR_F(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 PBR_ApplyNormalMap(vec3 worldPos, vec2 uv, vec3 n_geom, vec3 normal_ts)
{
    vec3 dp1 = dFdx(worldPos);
    vec3 dp2 = dFdy(worldPos);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    float inv_det = 1.0 / max(duv1.x * duv2.y - duv1.y * duv2.x, 1e-8);
    vec3 t = normalize((dp1 * duv2.y - dp2 * duv1.y) * inv_det);
    vec3 b = normalize((dp2 * duv1.x - dp1 * duv2.x) * inv_det);

    mat3 tbn = mat3(t, b, normalize(n_geom));
    return normalize(tbn * normal_ts);
}

// ─── surface entry ───

SurfaceOutput EvalSurface(SurfaceInput si, uint miID)
{
    MaterialInstance mi = mtl.mi[miID];

    vec4 albedo     = unpackUnorm4x8(mi.base_color);
    float metallic  = clamp(mi.metallic,  0.0,  1.0);
    float roughness = clamp(mi.roughness, 0.04, 1.0);

    vec3 N = normalize(si.worldNormal);
    vec3 V = si.viewDir;
    vec3 L = normalize(ULRE_GetSkyLightDir());
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0001);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);

    vec3  F = PBR_F(VdotH, F0);
    float D = PBR_NDF(NdotH, roughness);
    float G = PBR_G(NdotV, NdotL, roughness);

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.0001);

    vec3 kD      = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo.rgb / PBR_PI;

    vec3 skyBase    = max(sky.base_sky_color.rgb, vec3(0.08));
    vec3 sunColor   = max(ULRE_GetSkyLightColor(), sky.sun_color.rgb * max(sky.sun_intensity, 0.35));
    sunColor        = max(sunColor, vec3(0.30));
    vec3 skyAmbient = max(ULRE_GetSkyAmbientColor(), skyBase * 0.75);

    float wrappedNdotL = NdotL * 0.62 + 0.38;
    vec3 color = (diffuse + specular) * sunColor * wrappedNdotL;
    color += skyAmbient * albedo.rgb * (1.0 - metallic) * 0.85;
    color += skyBase * F0 * 0.16;
    color *= 1.18;

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

