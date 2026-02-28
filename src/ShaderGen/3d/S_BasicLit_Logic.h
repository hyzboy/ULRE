#pragma once

#include <hgl/graph/mtl/ShaderLogic.h>

#include "../common/MFSkyLight.h"

namespace hgl::graph::mtl {

constexpr const char BASIC_LIT_VS_BUSINESS[] = R"(
vec4 VertexShaderBusiness(const VertexInput vi)
{
    Output.TexCoord = vi.TexCoord;
    Output.Normal = normalize(mat3(camera.view * GetLocalToWorld()) * vi.Normal);
    Output.Position = camera.vp * GetLocalToWorld() * vec4(vi.Position, 1.0);
    return vec4(vi.Position, 1.0);
}
)";

constexpr const char BASIC_LIT_FS_BUSINESS[] = ULRE_SKYLIGHT_GLSL_COMMON R"(
#define ULRE_SURFACE_TEX_MODE_COLOR_ONLY 1
#define ULRE_SURFACE_TEX_MODE_COLOR_NORMAL 2
#define ULRE_SURFACE_TEX_MODE_COLOR_NORMAL_ROUGHNESS 3

#undef ULRE_SURFACE_TEX_MODE
#define ULRE_SURFACE_TEX_MODE ULRE_SURFACE_TEX_MODE_COLOR_NORMAL_ROUGHNESS


vec3 halfLambert(vec3 normal, vec3 lightDir)
{
    float NdotL = max(dot(normal, lightDir), 0.0);
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

    vec2 uv = ResolveSurfaceUV(Input.TexCoord);
    vec3 normal = ResolveSurfaceNormal(Input.Normal, uv, ResolveRuntimeNormalStrength(mi.normal_strength));
    vec3 viewDir = normalize(camera.pos - Input.Position.xyz);
    vec3 lightDir = normalize((camera.view * vec4(ULRE_GetSkyLightDir(), 0.0)).xyz);

    vec3 sampled_albedo = ResolveAlbedoColor(uv);
    vec4 base_color = unpackUnorm4x8(mi.base_color) * vec4(sampled_albedo, 1.0);

#if ULRE_SURFACE_TEX_MODE == ULRE_SURFACE_TEX_MODE_COLOR_ONLY
    return vec4(base_color.rgb, 1.0);
#endif

    float roughness_mix = ResolveSurfaceRoughness(mi.roughness, uv);

    vec3 diffuse = base_color.rgb * halfLambert(normal, lightDir);
    diffuse = max(diffuse, vec3(0.1));

    vec3 halfDir = normalize(lightDir + viewDir);
    float spec_power = mix(96.0, 8.0, roughness_mix);
    float spec = pow(max(dot(normal, halfDir), 0.0), spec_power) * mi.metallic;
    float fresnel = fresnelSchlick(max(dot(viewDir, halfDir), 0.0), mi.fresnel);

    vec3 sunColor = max(ULRE_GetSkyLightColor(), vec3(0.20));
    vec3 skyAmbient = ULRE_GetSkyAmbientColor();

    vec3 color = diffuse + spec * fresnel;
    color *= sunColor;
    color += skyAmbient * 0.25;

#if ULRE_SKYLIGHT_MODEL == ULRE_SKYLIGHT_MODEL_IBL
    color += mi.ibl_intensity * sky.base_sky_color.rgb;
#endif

    return vec4(color, 1.0);
}
)";

constexpr const char* BASIC_LIT_VERTEX_RESOURCES[] = {
    "camera",
    "l2w"
};

constexpr const char* BASIC_LIT_FRAGMENT_RESOURCES[] = {
    "camera",
    "sky",
    "mtl",
    "TextureBaseColor",
    "TextureNormal",
    "TextureRoughness"
};

constexpr const char* BASIC_LIT_FRAGMENT_HELPERS[] = {
    "GetMI"
};

const VertexShaderLogic BASIC_LIT_VERTEX_SHADER_LOGIC = {
    {
        BASIC_LIT_VS_BUSINESS,
        nullptr,
        BASIC_LIT_VERTEX_RESOURCES,
        2,
        nullptr,
        0
    }
};

const FragmentShaderLogic BASIC_LIT_FRAGMENT_SHADER_LOGIC = {
    {
        BASIC_LIT_FS_BUSINESS,
        nullptr,
        BASIC_LIT_FRAGMENT_RESOURCES,
        6,
        BASIC_LIT_FRAGMENT_HELPERS,
        1
    }
};

const MaterialLogicDef BASIC_LIT_LOGIC = {
    BASIC_LIT_VERTEX_SHADER_LOGIC,
    BASIC_LIT_FRAGMENT_SHADER_LOGIC,
    nullptr,
    nullptr,
    nullptr
};

}
