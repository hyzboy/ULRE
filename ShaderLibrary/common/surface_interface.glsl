
#ifndef SURFACE_INTERFACE_GLSL
#define SURFACE_INTERFACE_GLSL


struct SurfaceInput
{
    vec3 worldPos;           vec3 worldNormal;
    vec2 uv0;
    vec2 uv1;
    vec4 vertexColor;
    vec3 viewDir;            vec2 screenPos;
    float luminance;     };

struct SurfaceOutput
{
    vec3  baseColor;
    vec3  normal;
    float metallic;
    float roughness;
    float ao;
    vec3  emissive;
    float alpha;
};

struct SurfaceOutputExt
{
    vec3  subsurfaceColor;
    float subsurfacePower;
    float thickness;
    vec3  sheenColor;
    float sheenRoughness;
    float clearCoat;
    float clearCoatRoughness;
    vec3  clearCoatNormal;
    float anisotropy;
    vec3  anisotropyDirection;
};

#endif 