#ifndef ULRE_SURFACE_PBRCOLOR3D_SURFACE_GLSL
#define ULRE_SURFACE_PBRCOLOR3D_SURFACE_GLSL

#include "common/surface_interface.glsl"
#include "common/ssbo_material_instance.glsl"
#include "util/pbr_brdf.glsl"
#include "util/normal_mapping.glsl"


SurfaceOutput EvalSurface(SurfaceInput si)
{
    MaterialBindingInstance mi = GetMaterialBindingInstance();

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
    float alpha2 = roughness * roughness * roughness * roughness;  // r^4 (GGX convention)

    vec3  F = PBR_F_Schlick(VdotH, F0);
    float D = PBR_D_GGX(NdotH, alpha2);
    float G = PBR_G_Smith(NdotV, NdotL, roughness);

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

#endif // ULRE_SURFACE_PBRCOLOR3D_SURFACE_GLSL
