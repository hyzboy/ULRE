#include <hgl/mtl/ShaderCodeModule.h>

#include <hgl/type/StrChar.h>

namespace hgl::graph::mtl
{
    uint64 GetShaderCodeModuleDefinitionHash(
        const ShaderCodeModuleDefinition &definition) noexcept
    {
        hgl::hash::FNV1aHasher64 h;

        h << definition.name
          << definition.glsl_code;
        h << definition.kind
          << definition.priority
          << definition.flags;
        if (definition.slot_role != ShaderModuleSlotRole::Unknown
         || definition.provided_capabilities != 0
         || definition.required_capabilities != 0)
        {
            h << definition.slot_role
              << definition.provided_capabilities
              << definition.required_capabilities;
        }

        h << definition.ssbo_requirement_count;
        for (uint32 i = 0; i < definition.ssbo_requirement_count; ++i)
        {
            const auto &requirement = definition.ssbo_requirements[i];
            h << requirement.name;
            h << requirement.ssbo_type
              << requirement.material_private_data_slot
              << requirement.stage_flags
              << requirement.required
              << requirement.allow_fallback;
        }

        h << definition.texture_layer_requirement_count;
        for (uint32 i = 0; i < definition.texture_layer_requirement_count; ++i)
            h << definition.texture_layer_requirements[i];

        h << definition.semantic_requirement_count;
        for (uint32 i = 0; i < definition.semantic_requirement_count; ++i)
            h << definition.semantic_requirements[i];

        h << definition.semantic_provide_count;
        for (uint32 i = 0; i < definition.semantic_provide_count; ++i)
            h << definition.semantic_provides[i];

        h << definition.dependency_count;
        for (uint32 i = 0; i < definition.dependency_count; ++i)
        {
            const ShaderCodeModuleDependency &dependency =
                definition.dependencies[i];
            h << dependency.module_name;
        }

        h << definition.module_conflict_count;
        for (uint32 i = 0; i < definition.module_conflict_count; ++i)
            h << definition.module_conflict_names[i];

        return h;
    }
}
