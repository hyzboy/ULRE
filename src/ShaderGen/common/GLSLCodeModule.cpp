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
                // 天光算法已统一到 ShaderLibrary/sky/sky_atmosphere.glsl（GetSky* 系列），
                // 由 CompositorAssembler 的 sky_module 选择注入。本模块仅作为
                // has_sky_root 标记，不再注入 GLSL 代码。
                "// Sky lighting provided by sky/sky_atmosphere.glsl (compositor sky module)",
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
                // 仅声明 SkyInfo UBO 资源需求；函数由 sky/sky_atmosphere.glsl 提供。
                "// SkyLightSimple: requires SkyInfo UBO (functions in sky/sky_atmosphere.glsl)",
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
                // CubeMap mode resources; implementation is selected by the
                // compositor sky module path.
                "// SkyLightCubeMap: requires SkyInfo UBO + SkyCubemap samplerCube",
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
               GLSLCodeModuleID::PBRSurface,
               "PBRSurface",
                "",
                nullptr, 0, nullptr, 0,
                PBR_TEXTURE_REQUIREMENTS,
                uint32(sizeof(PBR_TEXTURE_REQUIREMENTS) / sizeof(PBR_TEXTURE_REQUIREMENTS[0])),
                nullptr,
                0
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

    uint64 GetGLSLCodeModuleDefinitionHash(const GLSLCodeModuleID id) noexcept
    {
        const GLSLCodeModuleDefinition *definition = FindGLSLCodeModuleDefinition(id);
        if (!definition)
            return 0;

        uint64 hash = hgl::hash::FNV1aInit<uint64>();
        hash = hgl::hash::FNV1aAppendValueBytes(hash, definition->id);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, definition->kind);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, definition->priority);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, definition->flags);

        for (uint32 i = 0; i < definition->semantic_requirement_count; ++i)
            hash = hgl::hash::FNV1aAppendValueBytes(hash, definition->semantic_requirements[i]);

        for (uint32 i = 0; i < definition->semantic_provide_count; ++i)
            hash = hgl::hash::FNV1aAppendValueBytes(hash, definition->semantic_provides[i]);

        for (uint32 i = 0; i < definition->code_module_requirement_count; ++i)
            hash = hgl::hash::FNV1aAppendValueBytes(hash, definition->code_module_requirements[i]);

        return hash;
    }
}
