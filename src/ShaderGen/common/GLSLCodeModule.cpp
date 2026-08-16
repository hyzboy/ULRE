#include <hgl/graph/glsl/GLSLCodeModule.h>

#include <hgl/type/StrChar.h>

namespace hgl::graph::mtl
{
    uint64 GetGLSLCodeModuleDefinitionHash(
        const GLSLCodeModuleDefinition &definition) noexcept
    {
        hgl::hash::FNV1aHasher64 h;

        h << definition.name
          << definition.glsl_code;
        h << definition.kind
          << definition.priority
          << definition.flags
          << definition.metadata_version;

        h << definition.ubo_requirement_count;
        for (uint32 i = 0; i < definition.ubo_requirement_count; ++i)
            h << definition.ubo_requirements[i];

        h << definition.ssbo_requirement_count;
        for (uint32 i = 0; i < definition.ssbo_requirement_count; ++i)
        {
            const auto &requirement = definition.ssbo_requirements[i];
            h << requirement.name;
            h << requirement.ssbo_type
              << requirement.data_slot
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
            const GLSLCodeModuleDependency &dependency =
                definition.dependencies[i];
            h << dependency.module_name
              << dependency.min_metadata_version
              << dependency.max_metadata_version;
        }

        h << definition.condition_count;
        for (uint32 i = 0; i < definition.condition_count; ++i)
        {
            const GLSLCodeModuleCondition &condition = definition.conditions[i];
            h << condition.domain
              << condition.operation
              << condition.key
              << condition.value;
        }

        h << definition.module_conflict_count;
        for (uint32 i = 0; i < definition.module_conflict_count; ++i)
            h << definition.module_conflict_names[i];

        return h;
    }
}
