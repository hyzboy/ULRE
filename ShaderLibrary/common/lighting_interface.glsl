// @ulre begin
// @ulre name lighting_interface
// @ulre kind Shared
// @ulre priority 0
// @ulre end
// Lighting Interface — 定义统一直接光与间接光函数契约
#ifndef LIGHTING_INTERFACE_GLSL
#define LIGHTING_INTERFACE_GLSL

// All values required by a lighting algorithm are resolved before the
// algorithm is called. The source of baseColor, normal, or environment data
// is intentionally not part of this contract.
struct LightingInput
{
    vec3  baseColor;
    vec3  normal;
    vec3  viewDir;
    float metallic;
    float roughness;
    float fresnel;
    float ao;
    vec3  emissive;
    float alpha;

    vec3  mainLightDir;
    vec3  mainLightColor;
    vec3  ambientColor;
    vec3  reflectionColor;
};

// A lighting algorithm consumes only LightingInput and returns final RGBA.
//   vec4 EvalLighting(LightingInput lighting)

// 光照基础 Math 辅助库
float ULRE_LIT_D_GGX(float NdotH, float alpha2)
{
    float d = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    return alpha2 / (3.14159265 * d * d + 1e-7);
}

float ULRE_LIT_G_Smith(float NdotV, float NdotL, float roughness)
{
    float k  = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float gv = NdotV / (NdotV * (1.0 - k) + k + 1e-7);
    float gl = NdotL / (NdotL * (1.0 - k) + k + 1e-7);
    return gv * gl;
}

vec3 ULRE_LIT_F_Schlick(float VdotH, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

#endif // LIGHTING_INTERFACE_GLSL
