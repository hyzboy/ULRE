#include <hgl/graph/glsl/GLSLCodeModule.h>

#include <hgl/common/RenderAssignDef.h>

namespace hgl::graph::mtl
{
    namespace
    {
        constexpr GLSLCodeModuleUBORequirement SKY_LIGHT_UBO_REQUIREMENT[] =
        {
            { UBODescriptorSemantic::SkyInfo, uint32(VK_SHADER_STAGE_FRAGMENT_BIT) }
        };

        constexpr GLSLCodeModuleTextureRequirement SKY_LIGHT_CUBEMAP_TEXTURE_REQUIREMENT[] =
        {
            {
                "SkyCubemap",
                "samplerCube",
                DescriptorSemantic::SkyCubemapSampler,
                TextureSlot::Custom0,
                uint32(VK_SHADER_STAGE_FRAGMENT_BIT),
                true
            }
        };

        constexpr GLSLCodeModuleID SKY_LIGHT_HEADER_MODULES[] =
        {
            GLSLCodeModuleID::SkyLightHeader
        };

        constexpr GLSLCodeModuleID PBR_BRDF_MODULES[] =
        {
            GLSLCodeModuleID::BRDF
        };

        constexpr GLSLCodeModuleTextureRequirement PBR_TEXTURE_REQUIREMENTS[] =
        {
            { "TextureBaseColor", "sampler2D", DescriptorSemantic::MaterialTexture, TextureSlot::BaseColor, uint32(VK_SHADER_STAGE_FRAGMENT_BIT), false },
            { "TextureNormal", "sampler2D", DescriptorSemantic::MaterialTexture, TextureSlot::Normal, uint32(VK_SHADER_STAGE_FRAGMENT_BIT), false },
            { "TextureMetallic", "sampler2D", DescriptorSemantic::MaterialTexture, TextureSlot::Metallic, uint32(VK_SHADER_STAGE_FRAGMENT_BIT), false },
            { "TextureRoughness", "sampler2D", DescriptorSemantic::MaterialTexture, TextureSlot::Roughness, uint32(VK_SHADER_STAGE_FRAGMENT_BIT), false },
            { "TextureOcclusion", "sampler2D", DescriptorSemantic::MaterialTexture, TextureSlot::Occlusion, uint32(VK_SHADER_STAGE_FRAGMENT_BIT), false },
        };

        constexpr GLSLCodeModuleDefinition MODULES[] =
        {
            {
                GLSLCodeModuleID::SkyLightHeader,
                "SkyLightHeader",
                R"GLSL(
vec3 ULRE_GetSkyLightDir()
{
    return normalize(sky.sun_direction.xyz);
}
)GLSL",
                nullptr,
                0,
                nullptr,
                0,
                nullptr,
                0,
                nullptr,
                0
            },
            {
                GLSLCodeModuleID::SkyLightSimple,
                "SkyLightSimple",
                R"GLSL(
vec3 ULRE_GetSkyLightColor()
{
    return sky.sun_color.rgb * sky.sun_intensity;
}
                vec3 ULRE_GetSkyAmbientColor()
                {
                    float h = clamp(normalize(sky.sun_direction.xyz).z * 0.5 + 0.5, 0.0, 1.0);
                    return sky.base_sky_color.rgb * exp2(-(1.0 - h) * 0.8);
                }
                )GLSL",
                SKY_LIGHT_UBO_REQUIREMENT,
                uint32(sizeof(SKY_LIGHT_UBO_REQUIREMENT) / sizeof(SKY_LIGHT_UBO_REQUIREMENT[0])),
                nullptr,
                0,
                nullptr,
                0,
                SKY_LIGHT_HEADER_MODULES,
                uint32(sizeof(SKY_LIGHT_HEADER_MODULES) / sizeof(SKY_LIGHT_HEADER_MODULES[0]))
            },
            {
                GLSLCodeModuleID::SkyLightCubeMap,
                "SkyLightCubeMap",
                R"GLSL(
vec3 ULRE_GetSkyLightColor()
{
    return texture(SkyCubemap, ULRE_GetSkyLightDir()).rgb;
}
vec3 ULRE_GetSkyAmbientColor()
{
    return texture(SkyCubemap, ULRE_GetSkyLightDir()).rgb;
}
)GLSL",
                SKY_LIGHT_UBO_REQUIREMENT,
                uint32(sizeof(SKY_LIGHT_UBO_REQUIREMENT) / sizeof(SKY_LIGHT_UBO_REQUIREMENT[0])),
                nullptr,
                0,
                SKY_LIGHT_CUBEMAP_TEXTURE_REQUIREMENT,
                uint32(sizeof(SKY_LIGHT_CUBEMAP_TEXTURE_REQUIREMENT) / sizeof(SKY_LIGHT_CUBEMAP_TEXTURE_REQUIREMENT[0])),
                SKY_LIGHT_HEADER_MODULES,
                uint32(sizeof(SKY_LIGHT_HEADER_MODULES) / sizeof(SKY_LIGHT_HEADER_MODULES[0]))
            },
            {
                GLSLCodeModuleID::BRDF,
                "BRDF",
                R"GLSL(
float ULRE_D_GGX(float NdotH, float alpha2)
{
    float d = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    return alpha2 / (3.14159265 * d * d + 1e-7);
}
float ULRE_G_Smith(float NdotV, float NdotL, float roughness)
{
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float gv = NdotV / (NdotV * (1.0 - k) + k + 1e-7);
    float gl = NdotL / (NdotL * (1.0 - k) + k + 1e-7);
    return gv * gl;
}
vec3 ULRE_F_Schlick(float VdotH, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}
)GLSL",
                nullptr, 0, nullptr, 0, nullptr, 0,
                nullptr, 0
            },
            {
                GLSLCodeModuleID::PBRSurface,
                "PBRSurface",
                "",
                nullptr, 0, nullptr, 0,
                PBR_TEXTURE_REQUIREMENTS,
                uint32(sizeof(PBR_TEXTURE_REQUIREMENTS) / sizeof(PBR_TEXTURE_REQUIREMENTS[0])),
                PBR_BRDF_MODULES,
                uint32(sizeof(PBR_BRDF_MODULES) / sizeof(PBR_BRDF_MODULES[0]))
            }
        };
    }

    const GLSLCodeModuleDefinition *FindGLSLCodeModuleDefinition(const GLSLCodeModuleID id) noexcept
    {
        const uint32 index = static_cast<uint32>(id);
        return index < static_cast<uint32>(sizeof(MODULES) / sizeof(MODULES[0]))
            ? &MODULES[index]
            : nullptr;
    }

    const char *GetGLSLCodeModuleName(const GLSLCodeModuleID id) noexcept
    {
        const GLSLCodeModuleDefinition *definition = FindGLSLCodeModuleDefinition(id);
        return definition ? definition->name : "Unknown";
    }
}
