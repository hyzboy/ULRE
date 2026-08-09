#include <hgl/graph/glsl/GLSLCodeModule.h>

#include <hgl/common/RenderAssignDef.h>
#include <hgl/type/StrChar.h>

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
               nullptr, 0,
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

    bool TryGetGLSLCodeModuleIDByName(const char *name, GLSLCodeModuleID &out) noexcept
    {
        if (!name || !*name)
            return false;

        constexpr uint32 module_count =
            static_cast<uint32>(sizeof(MODULES) / sizeof(MODULES[0]));
        for (uint32 i = 0; i < module_count; ++i)
        {
            if (MODULES[i].name && hgl::strcmp(MODULES[i].name, name) == 0)
            {
                out = MODULES[i].id;
                return true;
            }
        }

        return false;
    }

    const char *GetGLSLCodeModuleName(const GLSLCodeModuleID id) noexcept
    {
        const GLSLCodeModuleDefinition *definition = FindGLSLCodeModuleDefinition(id);
        return definition ? definition->name : "Unknown";
    }

    uint64 GetGLSLCodeModuleDefinitionHash(
        const GLSLCodeModuleDefinition &definition) noexcept
    {
        uint64 hash = hgl::hash::FNV1aInit<uint64>();
        const auto append_string = [](uint64 current, const char *value) -> uint64
        {
            const uint32 length = value
                ? static_cast<uint32>(std::strlen(value)) : 0u;
            current = hgl::hash::FNV1aAppendValueBytes(current, length);
            if (length > 0)
                current = hgl::hash::FNV1aAppendBytes(current, value, length);
            return current;
        };

        hash = append_string(hash, definition.name);
        hash = append_string(hash, definition.glsl_code);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, definition.id);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, definition.kind);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, definition.priority);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, definition.flags);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, definition.metadata_version);

        hash = hgl::hash::FNV1aAppendValueBytes(hash, definition.ubo_requirement_count);
        for (uint32 i = 0; i < definition.ubo_requirement_count; ++i)
            hash = hgl::hash::FNV1aAppendValueBytes(hash, definition.ubo_requirements[i]);

        hash = hgl::hash::FNV1aAppendValueBytes(hash, definition.ssbo_requirement_count);
        for (uint32 i = 0; i < definition.ssbo_requirement_count; ++i)
        {
            const auto &requirement = definition.ssbo_requirements[i];
            hash = append_string(hash, requirement.name);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, requirement.ssbo_type);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, requirement.data_slot);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, requirement.stage_flags);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, requirement.required);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, requirement.allow_fallback);
        }

        hash = hgl::hash::FNV1aAppendValueBytes(hash, definition.texture_requirement_count);
        for (uint32 i = 0; i < definition.texture_requirement_count; ++i)
        {
            const auto &requirement = definition.texture_requirements[i];
            hash = append_string(hash, requirement.name);
            hash = append_string(hash, requirement.glsl_type);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, requirement.semantic);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, requirement.slot);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, requirement.stage_flags);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, requirement.required);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, requirement.allow_fallback);
        }

        hash = hgl::hash::FNV1aAppendValueBytes(hash, definition.texture_layer_requirement_count);
        for (uint32 i = 0; i < definition.texture_layer_requirement_count; ++i)
            hash = hgl::hash::FNV1aAppendValueBytes(hash, definition.texture_layer_requirements[i]);

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, definition.semantic_requirement_count);
        for (uint32 i = 0; i < definition.semantic_requirement_count; ++i)
            hash = hgl::hash::FNV1aAppendValueBytes(hash, definition.semantic_requirements[i]);

        hash = hgl::hash::FNV1aAppendValueBytes(hash, definition.semantic_provide_count);
        for (uint32 i = 0; i < definition.semantic_provide_count; ++i)
            hash = hgl::hash::FNV1aAppendValueBytes(hash, definition.semantic_provides[i]);

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, definition.code_module_requirement_count);
        for (uint32 i = 0; i < definition.code_module_requirement_count; ++i)
            hash = hgl::hash::FNV1aAppendValueBytes(hash, definition.code_module_requirements[i]);

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, definition.dependency_count);
        for (uint32 i = 0; i < definition.dependency_count; ++i)
        {
            const GLSLCodeModuleDependency &dependency =
                definition.dependencies[i];
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, dependency.module_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, dependency.min_metadata_version);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, dependency.max_metadata_version);
        }

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, definition.condition_count);
        for (uint32 i = 0; i < definition.condition_count; ++i)
        {
            const GLSLCodeModuleCondition &condition = definition.conditions[i];
            hash = hgl::hash::FNV1aAppendValueBytes(hash, condition.domain);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, condition.operation);
            hash = append_string(hash, condition.key);
            hash = append_string(hash, condition.value);
        }

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, definition.module_conflict_count);
        for (uint32 i = 0; i < definition.module_conflict_count; ++i)
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.module_conflicts[i]);

        return hash;
    }

    uint64 GetGLSLCodeModuleDefinitionHash(const GLSLCodeModuleID id) noexcept
    {
        const GLSLCodeModuleDefinition *definition = FindGLSLCodeModuleDefinition(id);
        return definition ? GetGLSLCodeModuleDefinitionHash(*definition) : 0;
    }
}
