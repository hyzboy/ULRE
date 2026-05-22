#ifndef ULRE_SURFACE_STANDARD_SURFACE_GLSL
#define ULRE_SURFACE_STANDARD_SURFACE_GLSL

// @sfm:surface_type    Standard
// @sfm:supports_phase  ForwardOpaque ForwardMasked GBuffer
// @sfm:require va      Normal TexCoord
// @sfm:optional va     Tangent
// @sfm:derive va       Tangent
// @sfm:require tex     BaseColor NormalMap
// @sfm:require ubo     camera lighting
// @sfm:require sky     false

#include "common/surface_interface.glsl"
#include "common/ssbo_material_instance.glsl"
#include "util/pbr_brdf.glsl"

SurfaceOutput EvalSurface(SurfaceInput si)
{
    MaterialBindingInstance mi = GetMaterialBindingInstance();

    vec3 N = normalize(si.worldNormal);
    vec3 V = si.viewDir;
    vec3 L = normalize(ULRE_GetSkyLightDir());

    vec3 sunColor   = max(ULRE_GetSkyLightColor(), vec3(0.2));
    vec3 skyAmbient = ULRE_GetSkyAmbientColor();

    vec3 albedo = unpackUnorm4x8(mi.base_color).rgb;
    albedo *= GetSamplerBaseColor(GetMaterialInstanceID(), si.uv0).rgb;

    float metallic  = clamp(mi.metallic,  0.0, 1.0);
    float roughness = clamp(mi.roughness, 0.04, 1.0);

    vec3 nm = GetSamplerNormal(GetMaterialInstanceID(), si.uv0).xyz * 2.0 - 1.0;
    nm.y = -nm.y;
    N = normalize(N + vec3(nm.xy, 0.0) * mi.normal_scale);

    vec2 mr    = vec2(1.0,1.0);//GetSamplerRoughness(si.uv0).rg;
    metallic   = clamp(metallic  * mr.r, 0.0, 1.0);
    roughness  = clamp(roughness * mr.g, 0.04, 1.0);

    float NdotL  = max(dot(N, L), 0.0);
    float NdotV  = max(dot(N, V), 1e-4);
    vec3  H      = normalize(V + L);
    float NdotH  = max(dot(N, H), 0.0);
    float VdotH  = max(dot(V, H), 0.0);

    float alpha2 = roughness * roughness * roughness * roughness;
    float D      = PBR_D_GGX(NdotH, alpha2);
    float G      = PBR_G_Smith(NdotV, NdotL, roughness);
    vec3  F0     = mix(vec3(0.04), albedo, metallic);
    vec3  F      = PBR_F_Schlick(VdotH, F0);

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

float EvalAlpha(SurfaceInput si)
{
    return 1.0;
}

#endif // ULRE_SURFACE_STANDARD_SURFACE_GLSL
