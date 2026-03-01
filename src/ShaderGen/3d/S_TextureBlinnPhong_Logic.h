#pragma once

#include <hgl/shadergen/ShaderLogic.h>

#include "../common/MFSkyLight.h"

namespace hgl::graph::mtl {

constexpr const char TEXTURE_BLINN_PHONG_VS_BUSINESS[] = R"(
vec4 VertexShaderBusiness(const VertexInput vi)
{
    Output.TexCoord = vi.TexCoord;
    Output.Normal = normalize(mat3(camera.view * GetLocalToWorld()) * vi.Normal);
    Output.Position = camera.vp * GetLocalToWorld() * vec4(vi.Position, 1.0);
    return vec4(vi.Position, 1.0);
}
)";

constexpr const char TEXTURE_BLINN_PHONG_FS_BUSINESS[] = R"(
#define ULRE_SURFACE_TEX_MODE_COLOR_ONLY 1
#define ULRE_SURFACE_TEX_MODE_COLOR_NORMAL 2
#define ULRE_SURFACE_TEX_MODE_COLOR_NORMAL_ROUGHNESS 3

#undef ULRE_SURFACE_TEX_MODE
#define ULRE_SURFACE_TEX_MODE ULRE_SURFACE_TEX_MODE_COLOR_NORMAL_ROUGHNESS


vec3 halfLambert(vec3 n, vec3 l)
{
    float NdotL = max(dot(n, l), 0.0);
    return vec3(NdotL * 0.5 + 0.5);
}

float fresnelSchlick(float cosTheta, float F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec2 ResolveSurfaceUV(vec2 uv)
{
    if (abs(uv.x) + abs(uv.y) < 0.0001)
        return vec2(0.5, 0.5);

    return fract(abs(uv));
}

vec3 ResolveAlbedoColor(vec2 uv)
{
    vec4 c = texture(TextureBaseColor, uv);
    vec3 rgb = c.rgb;

    if (max(max(rgb.r, rgb.g), rgb.b) < 0.0001)
    {
        vec4 center = texture(TextureBaseColor, vec2(0.5, 0.5));
        rgb = center.rgb;

        if (max(max(rgb.r, rgb.g), rgb.b) < 0.0001)
            rgb = vec3(max(c.a, center.a));
    }

    return rgb;
}

vec3 ResolveSurfaceNormal(vec3 input_normal, vec2 uv, float normal_strength)
{
#if ULRE_SURFACE_TEX_MODE >= ULRE_SURFACE_TEX_MODE_COLOR_NORMAL
    vec3 sampled_normal = texture(TextureNormal, uv).xyz * 2.0 - 1.0;
    sampled_normal.y = -sampled_normal.y;
    return normalize(input_normal + vec3(sampled_normal.xy, 0.0) * normal_strength);
#else
    return normalize(input_normal);
#endif
}

float ResolveRuntimeNormalStrength(float normal_strength)
{
    return normal_strength > 0.0001 ? normal_strength : 0.35;
}

float ResolveSurfaceRoughness(float base_roughness, vec2 uv)
{
#if ULRE_SURFACE_TEX_MODE >= ULRE_SURFACE_TEX_MODE_COLOR_NORMAL_ROUGHNESS
    float roughness_tex = texture(TextureRoughness, uv).r;
    return clamp(base_roughness * roughness_tex, 0.04, 1.0);
#else
    return clamp(base_roughness, 0.04, 1.0);
#endif
}

vec4 FragmentShaderBusiness()
{
    MaterialInstance mi = GetMI();

    const float spec_strength= 0.6;
    const float F0           = 0.04;

    vec2 uv = ResolveSurfaceUV(Input.TexCoord);
    vec4 base_color = vec4(ResolveAlbedoColor(uv), 1.0);

#if ULRE_SURFACE_TEX_MODE == ULRE_SURFACE_TEX_MODE_COLOR_ONLY
    return vec4(base_color.rgb, 1.0);
#endif

    float roughness = ResolveSurfaceRoughness(0.8, uv);
    float spec_power = mix(96.0, 8.0, roughness);

    vec3 n  = ResolveSurfaceNormal(Input.Normal, uv, ResolveRuntimeNormalStrength(mi.normal_strength));
    vec3 v  = vec3(0.0, 0.0, 1.0);
    vec3 l  = normalize((camera.view * vec4(ULRE_GetSkyLightDir(), 0.0)).xyz);

    vec3 h  = normalize(l + v);

    vec3 diffuse = base_color.rgb * halfLambert(n, l);

    float NdotH = max(dot(n, h), 0.0);
    float spec  = pow(NdotH, spec_power) * spec_strength;

    float F = fresnelSchlick(max(dot(v, h), 0.0), F0);

    vec3 sunColor = max(ULRE_GetSkyLightColor(), vec3(0.20));
    vec3 skyAmbient = ULRE_GetSkyAmbientColor();

    vec3 color = diffuse + spec * F;
    color *= sunColor;
    color += skyAmbient * 0.25;

    return vec4(color, 1.0);
}
)";

constexpr const char* TEXTURE_BLINN_PHONG_VERTEX_RESOURCES[] = {
    "camera",
    "l2w"
};

constexpr const char* TEXTURE_BLINN_PHONG_FRAGMENT_RESOURCES[] = {
    "camera",
    "sky",
    "mtl",
    "TextureBaseColor",
    "TextureNormal",
    "TextureRoughness"
};

constexpr const char* TEXTURE_BLINN_PHONG_FRAGMENT_HELPERS[] = {
    "GetMI"
};

const VertexShaderLogic TEXTURE_BLINN_PHONG_VERTEX_SHADER_LOGIC = {
    {
        TEXTURE_BLINN_PHONG_VS_BUSINESS,
        nullptr,
        TEXTURE_BLINN_PHONG_VERTEX_RESOURCES,
        2,
        nullptr,
        0
    }
};

const FragmentShaderLogic TEXTURE_BLINN_PHONG_FRAGMENT_SHADER_LOGIC = {
    {
        TEXTURE_BLINN_PHONG_FS_BUSINESS,
        nullptr,
        TEXTURE_BLINN_PHONG_FRAGMENT_RESOURCES,
        6,
        TEXTURE_BLINN_PHONG_FRAGMENT_HELPERS,
        1
    }
};

const MaterialLogicDef TEXTURE_BLINN_PHONG_LOGIC = {
    TEXTURE_BLINN_PHONG_VERTEX_SHADER_LOGIC,
    TEXTURE_BLINN_PHONG_FRAGMENT_SHADER_LOGIC,
    nullptr,
    nullptr,
    nullptr
};

}
