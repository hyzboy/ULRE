// Lighting — 分级光照计算 (EvalLighting 供 main_forward_opaque.frag.glsl 路径使用)
// QUALITY_TIER <= 1 : Lambert
// QUALITY_TIER 2~3  : Half-Lambert + Blinn-Phong (Low)
// QUALITY_TIER >= 4 : Simplified Cook-Torrance PBR, 无 IBL/cubemap (High)

#include "common/surface_interface.glsl"

#if QUALITY_TIER >= 4

float EL_D_GGX(float NdotH, float alpha2)
{
    float d = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    return alpha2 / (3.14159265 * d * d + 1e-7);
}

float EL_G_Smith(float NdotV, float NdotL, float roughness)
{
    float k  = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float gv = NdotV / (NdotV * (1.0 - k) + k + 1e-7);
    float gl = NdotL / (NdotL * (1.0 - k) + k + 1e-7);
    return gv * gl;
}

vec3 EL_F_Schlick(float VdotH, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

#endif // QUALITY_TIER >= 4

vec3 EvalLighting(SurfaceOutput surface, vec3 viewDir, vec3 lightDir, vec3 lightColor)
{
    vec3 N = surface.normal;
    vec3 V = viewDir;
    vec3 L = lightDir;

#if QUALITY_TIER <= 1
    // Simple Lambert
    float NdotL = max(dot(N, L), 0.0);
    return surface.baseColor * lightColor * NdotL;

#elif QUALITY_TIER <= 3
    // Half-Lambert + Blinn-Phong
    float h         = dot(N, L) * 0.5 + 0.5;
    float hl        = h * h;
    vec3  H         = normalize(V + L);
    float shininess = mix(256.0, 8.0, surface.roughness);
    float spec      = pow(max(dot(N, H), 0.0), shininess);
    float specScale = surface.metallic * (1.0 - surface.roughness * 0.9);
    vec3  specColor = mix(vec3(spec), surface.baseColor * spec, surface.metallic);
    return surface.baseColor * hl * lightColor + specColor * specScale * lightColor;

#else
    // Simplified Cook-Torrance PBR (no IBL, no cubemap)
    float NdotL  = max(dot(N, L), 0.0);
    float NdotV  = max(dot(N, V), 1e-4);
    vec3  H      = normalize(V + L);
    float NdotH  = max(dot(N, H), 0.0);
    float VdotH  = max(dot(V, H), 0.0);

    float alpha2 = surface.roughness * surface.roughness * surface.roughness * surface.roughness;
    float D      = EL_D_GGX(NdotH, alpha2);
    float G      = EL_G_Smith(NdotV, NdotL, surface.roughness);
    vec3  F0     = mix(vec3(0.04), surface.baseColor, surface.metallic);
    vec3  F      = EL_F_Schlick(VdotH, F0);

    vec3 kd       = (1.0 - F) * (1.0 - surface.metallic);
    vec3 diffuse  = kd * surface.baseColor / 3.14159265 * NdotL;
    vec3 specular = D * G * F / max(4.0 * NdotV * NdotL, 1e-4) * NdotL;

    return (diffuse + specular) * lightColor;
#endif
}
