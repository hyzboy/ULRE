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
)GLSL",
                SKY_LIGHT_UBO_REQUIREMENT,
                uint32(sizeof(SKY_LIGHT_UBO_REQUIREMENT) / sizeof(SKY_LIGHT_UBO_REQUIREMENT[0])),
                nullptr,
                0,
                SKY_LIGHT_CUBEMAP_TEXTURE_REQUIREMENT,
                uint32(sizeof(SKY_LIGHT_CUBEMAP_TEXTURE_REQUIREMENT) / sizeof(SKY_LIGHT_CUBEMAP_TEXTURE_REQUIREMENT[0])),
                SKY_LIGHT_HEADER_MODULES,
                uint32(sizeof(SKY_LIGHT_HEADER_MODULES) / sizeof(SKY_LIGHT_HEADER_MODULES[0]))
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
