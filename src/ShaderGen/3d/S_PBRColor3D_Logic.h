#pragma once

#include <hgl/shadergen/ShaderLogic.h>
#include "../common/MFSkyLight.h"

namespace hgl::graph::mtl {

// ─────────────────────────────────────────────────────────────────────────────
// Vertex Shader
//   Outputs world-space Normal and world-space Position so the FS can
//   compute a correct view direction (camera.pos - world_pos) and use
//   the sky light direction directly in world space.
// ─────────────────────────────────────────────────────────────────────────────

constexpr const char PBR_COLOR_3D_VS_BUSINESS[] = R"(
vec4 VertexShaderBusiness(const VertexInput vi)
{
    Output.TexCoord = vi.TexCoord;
    Output.Normal   = normalize(mat3(GetLocalToWorld()) * vi.Normal);
    Output.Position = GetLocalToWorld() * vec4(vi.Position, 1.0);
    return vec4(vi.Position, 1.0);
}
)";

// ─────────────────────────────────────────────────────────────────────────────
// Fragment Shader — Cook-Torrance PBR BRDF (Texture2DArray baseColor + normal)
//
// MI layout (must match mi_codes in M_PBRColor3D.cpp):
//   uint  base_color   — RGBA packed (unpackUnorm4x8 to recover vec4)
//   float metallic     — [0, 1]
//   float roughness    — [0.04, 1]
//   uint  texture_id   — Texture2DArray layer index
//
// Lighting model:
//   NDF  : GGX / Trowbridge-Reitz
//   G    : Smith / Schlick-GGX
//   F    : Schlick Fresnel
//   kD   : Lambertian diffuse, energy-conserved, no diffuse for metals
//   Light: sky directional + sky ambient
// ─────────────────────────────────────────────────────────────────────────────

constexpr const char PBR_COLOR_3D_FS_BUSINESS[] = R"(
const float PBR_PI = 3.14159265359;

// GGX Normal Distribution Function
float PBR_NDF(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PBR_PI * d * d, 0.0001);
}

// Schlick-GGX Geometry term (single direction)
float PBR_G1(float NdotX, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / max(NdotX * (1.0 - k) + k, 0.0001);
}

// Smith combined geometry term
float PBR_G(float NdotV, float NdotL, float roughness)
{
    return PBR_G1(NdotV, roughness) * PBR_G1(NdotL, roughness);
}

// Fresnel-Schlick approximation
vec3 PBR_F(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Build a TBN frame from derivatives, so tangent-space normal maps can be
// applied without requiring explicit vertex tangents.
vec3 PBR_ApplyNormalMap(vec2 uv, vec3 n_geom, vec3 normal_ts)
{
    vec3 dp1 = dFdx(Input.Position.xyz);
    vec3 dp2 = dFdy(Input.Position.xyz);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    float inv_det = 1.0 / max(duv1.x * duv2.y - duv1.y * duv2.x, 1e-8);
    vec3 t = normalize((dp1 * duv2.y - dp2 * duv1.y) * inv_det);
    vec3 b = normalize((dp2 * duv1.x - dp1 * duv2.x) * inv_det);

    mat3 tbn = mat3(t, b, normalize(n_geom));
    return normalize(tbn * normal_ts);
}

vec4 FragmentShaderBusiness()
{
    MaterialInstance mi = GetMI();

    vec2 uv = fract(abs(Input.TexCoord));
    vec4 tex_albedo = texture(TextureBaseColor, vec3(uv, float(mi.texture_id)));
    vec3 tex_normal = texture(TextureNormal, vec3(uv, float(mi.texture_id))).xyz * 2.0 - 1.0;
    vec4 mi_albedo  = unpackUnorm4x8(mi.base_color);
    vec4 albedo     = tex_albedo * mi_albedo;
    float metallic  = clamp(mi.metallic,  0.0,  1.0);
    float roughness = clamp(mi.roughness, 0.04, 1.0);

    // All vectors in world space
    vec3 N = PBR_ApplyNormalMap(uv, normalize(Input.Normal), normalize(tex_normal));
    vec3 V = normalize(camera.pos - Input.Position.xyz);
    vec3 L = normalize(ULRE_GetSkyLightDir());
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0001);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    // Dielectric base reflectance F0=0.04; metals use albedo
    vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);

    vec3  F = PBR_F(VdotH, F0);
    float D = PBR_NDF(NdotH, roughness);
    float G = PBR_G(NdotV, NdotL, roughness);

    // Cook-Torrance specular
    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.0001);

    // Lambertian diffuse (metals have no diffuse)
    vec3 kD      = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo.rgb / PBR_PI;

    vec3 skyBase    = max(sky.base_sky_color.rgb, vec3(0.08));
    vec3 sunColor   = max(ULRE_GetSkyLightColor(), sky.sun_color.rgb * max(sky.sun_intensity, 0.35));
    sunColor        = max(sunColor, vec3(0.30));
    vec3 skyAmbient = max(ULRE_GetSkyAmbientColor(), skyBase * 0.75);

    // Direct lighting
    float wrappedNdotL = NdotL * 0.62 + 0.38;
    vec3 color = (diffuse + specular) * sunColor * wrappedNdotL;

    // Ambient approximation (IBL-less fallback)
    color += skyAmbient * albedo.rgb * (1.0 - metallic) * 0.85;
    color += skyBase * F0 * 0.16;

    // Presentation mode: lift exposure to make material differences easier to read.
    color *= 1.18;

    return vec4(color, 1.0);
}
)";

// ─────────────────────────────────────────────────────────────────────────────
// Resource & helper declarations
// ─────────────────────────────────────────────────────────────────────────────

constexpr const char* PBR_COLOR_3D_VERTEX_RESOURCES[] = {
    "camera",
    "l2w"
};

constexpr const char* PBR_COLOR_3D_FRAGMENT_RESOURCES[] = {
    "camera",
    "sky",
    "mtl",
    "TextureBaseColor",
    "TextureNormal"
};

constexpr const char* PBR_COLOR_3D_FRAGMENT_HELPERS[] = {
    "GetMI"
};

// ─────────────────────────────────────────────────────────────────────────────
// Logic definitions
// ─────────────────────────────────────────────────────────────────────────────

const VertexShaderLogic PBR_COLOR_3D_VERTEX_SHADER_LOGIC = {
    {
        PBR_COLOR_3D_VS_BUSINESS,
        nullptr,
        PBR_COLOR_3D_VERTEX_RESOURCES,
        2,
        nullptr,
        0
    }
};

const FragmentShaderLogic PBR_COLOR_3D_FRAGMENT_SHADER_LOGIC = {
    {
        PBR_COLOR_3D_FS_BUSINESS,
        nullptr,
        PBR_COLOR_3D_FRAGMENT_RESOURCES,
        5,
        PBR_COLOR_3D_FRAGMENT_HELPERS,
        1
    }
};

const MaterialLogicDef PBR_COLOR_3D_LOGIC = {
    PBR_COLOR_3D_VERTEX_SHADER_LOGIC,
    PBR_COLOR_3D_FRAGMENT_SHADER_LOGIC,
    nullptr,
    nullptr,
    nullptr
};

}//namespace hgl::graph::mtl
