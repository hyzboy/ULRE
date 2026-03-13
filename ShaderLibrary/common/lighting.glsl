// Lighting — 分级光照计算
// QUALITY_TIER 0~1: Lambert
// QUALITY_TIER 2~3: BlinnPhong
// QUALITY_TIER 4+:  PBR (Cook-Torrance) — 暂 fallback BlinnPhong

#include "common/surface_interface.glsl"

vec3 EvalLighting(SurfaceOutput surface, vec3 viewDir, vec3 lightDir, vec3 lightColor)
{
#if QUALITY_TIER <= 1
    // Simple Lambert
    float NdotL = max(dot(surface.normal, lightDir), 0.0);
    return surface.baseColor * lightColor * NdotL;

#elif QUALITY_TIER <= 3
    // BlinnPhong
    float NdotL = max(dot(surface.normal, lightDir), 0.0);
    vec3 H = normalize(viewDir + lightDir);
    float NdotH = max(dot(surface.normal, H), 0.0);
    float spec = pow(NdotH, mix(8.0, 128.0, 1.0 - surface.roughness));
    vec3 diffuse = surface.baseColor * lightColor * NdotL;
    vec3 specular = lightColor * spec * surface.metallic;
    return diffuse + specular;

#else
    // PBR (Cook-Torrance) — 后续实现，暂 fallback BlinnPhong
    float NdotL = max(dot(surface.normal, lightDir), 0.0);
    vec3 H = normalize(viewDir + lightDir);
    float NdotH = max(dot(surface.normal, H), 0.0);
    float spec = pow(NdotH, mix(8.0, 128.0, 1.0 - surface.roughness));
    vec3 diffuse = surface.baseColor * lightColor * NdotL;
    vec3 specular = lightColor * spec * surface.metallic;
    return diffuse + specular;
#endif
}
