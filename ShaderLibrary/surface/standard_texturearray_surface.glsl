// standard_texturearray_surface.glsl — Standard Lit Surface with Texture2DArray sampling

#include "common/surface_interface.glsl"

#include "common/material_instance_ssbo.glsl"
struct MaterialInstance
{
    uint  base_color;
    float metallic;
    float roughness;
    float normal_scale;
    uint  texture_id;
};
MI_SSBO;

layout(set=MATERIAL_SET, binding=TEXALBEDO_BINDING) uniform sampler2DArray TexAlbedo;
#if QUALITY_TIER >= 2
layout(set=MATERIAL_SET, binding=TEXNORMAL_BINDING) uniform sampler2DArray TexNormal;
#endif
#if QUALITY_TIER >= 4
layout(set=MATERIAL_SET, binding=TEXMR_BINDING) uniform sampler2DArray TexMR;   // R=metallic, G=roughness
#endif

#include "common/skylight_simple.glsl"

float halfLambertDiffuse(vec3 N, vec3 L)
{
    float h = dot(N, L) * 0.5 + 0.5;
    return h * h;
}

#if QUALITY_TIER >= 4

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

#endif

SurfaceOutput EvalSurface(SurfaceInput si, uint miID)
{
    MaterialInstance mi = mtl.mi[miID];

    vec3 N = normalize(si.worldNormal);
    vec3 V = si.viewDir;
    vec3 L = normalize(ULRE_GetSkyLightDir());

    vec3 sunColor   = max(ULRE_GetSkyLightColor(), vec3(0.2));
    vec3 skyAmbient = ULRE_GetSkyAmbientColor();

    vec3 albedo = unpackUnorm4x8(mi.base_color).rgb;
    float layer = float(mi.texture_id);
    albedo *= texture(TexAlbedo, vec3(si.uv0, layer)).rgb;

    float metallic  = clamp(mi.metallic,  0.0, 1.0);
    float roughness = clamp(mi.roughness, 0.04, 1.0);

#if QUALITY_TIER >= 2
    vec3 nm = texture(TexNormal, vec3(si.uv0, layer)).xyz * 2.0 - 1.0;
    nm.y = -nm.y;
    N = normalize(N + vec3(nm.xy, 0.0) * mi.normal_scale);
#endif

#if QUALITY_TIER >= 4
    vec2 mr    = texture(TexMR, vec3(si.uv0, layer)).rg;
    metallic   = clamp(metallic  * mr.r, 0.0, 1.0);
    roughness  = clamp(roughness * mr.g, 0.04, 1.0);

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

#else
    float hl        = halfLambertDiffuse(N, L);
    vec3  H         = normalize(V + L);
    float shininess = mix(256.0, 8.0, roughness);
    float spec      = pow(max(dot(N, H), 0.0), shininess);
    float specScale = metallic * (1.0 - roughness * 0.9);
    vec3  specColor = mix(vec3(spec), albedo * spec, metallic);

    vec3 color  = albedo * hl * sunColor;
    color      += specColor * specScale * sunColor;
    color      += skyAmbient * albedo * 0.25;
#endif

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
