#include <hgl/mtl/ShaderCodeModuleMetadata.h>

#include <hgl/type/ValueArray.h>
#include <hgl/type/StrChar.h>

#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        bool SetMetadataFailure(
            ShaderCodeModuleMetadataValidationDiagnostic &out_diagnostic,
            const ShaderCodeModuleMetadataValidationError error,
            const char *module_name,
            const uint32 item_index = 0,
            const char *related_module_name = nullptr,
            const ShaderCodeModuleSemantic semantic =
                ShaderCodeModuleSemantic::Unknown) noexcept
        {
            out_diagnostic.error = error;
            out_diagnostic.module_name = module_name ? module_name : "";
            out_diagnostic.related_module_name =
                related_module_name ? related_module_name : "";
            out_diagnostic.semantic = semantic;
            out_diagnostic.item_index = item_index;
            return false;
        }

        bool HasValidArray(const void *data, const uint32 count) noexcept
        {
            return count == 0 || data != nullptr;
        }

        bool IsValidRequirement(
            const ShaderCodeModuleSemanticRequirement &requirement) noexcept
        {
            return requirement.source >=
                    ShaderCodeModuleCapabilitySource::GeometryAttribute
                && requirement.source <=
                    ShaderCodeModuleCapabilitySource::ProducedSemantic
                && requirement.semantic != ShaderCodeModuleSemantic::Unknown
                && requirement.numeric_class_mask != 0
                && requirement.min_component_count <= 4
                && requirement.max_component_count <= 4
                && (requirement.max_component_count == 0
                 || requirement.min_component_count
                    <= requirement.max_component_count);
        }

        bool SameName(const char *lhs, const char *rhs) noexcept
        {
            return lhs == rhs
                || (lhs && rhs && std::strcmp(lhs, rhs) == 0);
        }

        bool IsProviderCandidate(
            const ShaderCodeModuleDefinition &definition) noexcept
        {
            return definition.semantic_provide_count > 0
                && definition.kind != ShaderCodeModuleKind::Surface
                && definition.kind != ShaderCodeModuleKind::FragmentShader;
        }

        bool FindModuleIndex(
            const ShaderCodeModuleRegistry &registry,
            const char *name,
            int &out_index) noexcept
        {
            for (int index = 0; index < registry.GetCount(); ++index)
            {
                const ShaderCodeModuleDefinition *definition =
                    registry.GetModuleByIndex(index);
                if (definition && SameName(definition->name, name))
                {
                    out_index = index;
                    return true;
                }
            }

            return false;
        }

        bool ValidateDependencyVisit(
            const ShaderCodeModuleRegistry &registry,
            const int module_index,
            ValueArray<uint8> &visit_state,
            ShaderCodeModuleMetadataValidationDiagnostic &out_diagnostic) noexcept
        {
            if (visit_state[module_index] == 2)
                return true;
            if (visit_state[module_index] == 1)
                return false;

            visit_state[module_index] = 1;
            const ShaderCodeModuleDefinition *definition =
                registry.GetModuleByIndex(module_index);
            if (!definition)
                return false;

            const uint32 dependency_count =
                definition->dependency_count;
            for (uint32 dependency_index = 0;
                 dependency_index < dependency_count;
                 ++dependency_index)
            {
                ShaderCodeModuleDependency dependency =
                    definition->dependencies[dependency_index];

                int target_index = -1;
                if (!FindModuleIndex(registry, dependency.module_name, target_index))
                    continue;

                if (visit_state[target_index] == 1)
                {
                    return SetMetadataFailure(
                        out_diagnostic,
                        ShaderCodeModuleMetadataValidationError::DependencyCycle,
                        definition->name,
                        dependency_index,
                        dependency.module_name);
                }

                if (!ValidateDependencyVisit(
                        registry, target_index, visit_state, out_diagnostic))
                    return false;
            }

            visit_state[module_index] = 2;
            return true;
        }
    }

    const char *GetShaderCodeModuleMetadataValidationErrorName(
        const ShaderCodeModuleMetadataValidationError error) noexcept
    {
#define HGL_ERROR(name) case ShaderCodeModuleMetadataValidationError::name: return #name;
        switch (error)
        {
            HGL_GLSL_CODE_MODULE_METADATA_VALIDATION_ERROR_LIST
        }
#undef HGL_ERROR
        return "Unknown";
    }

    // 直通访问器——ShaderCodeResourceManifest 等公共调用方经此访问依赖表
    // （Metadata.cpp 内部已直接访问字段）
    uint32 GetNormalizedShaderCodeModuleDependencyCount(
        const ShaderCodeModuleDefinition &definition) noexcept
    {
        return definition.dependency_count;
    }

    bool GetNormalizedShaderCodeModuleDependency(
        const ShaderCodeModuleDefinition &definition,
        const uint32 index,
        ShaderCodeModuleDependency &out_dependency) noexcept
    {
        if (!definition.dependencies || index >= definition.dependency_count)
            return false;

        out_dependency = definition.dependencies[index];
        return true;
    }

    bool AreShaderCodeModulesConflicting(
        const ShaderCodeModuleDefinition &lhs,
        const ShaderCodeModuleDefinition &rhs) noexcept
    {
        if ((lhs.module_conflict_count > 0 && !lhs.module_conflict_names)
         || (rhs.module_conflict_count > 0 && !rhs.module_conflict_names))
            return false;

        for (uint32 i = 0; i < lhs.module_conflict_count; ++i)
        {
            if (SameName(lhs.module_conflict_names[i], rhs.name))
                return true;
        }

        for (uint32 i = 0; i < rhs.module_conflict_count; ++i)
        {
            if (SameName(rhs.module_conflict_names[i], lhs.name))
                return true;
        }

        return false;
    }

    bool ValidateShaderCodeModuleMetadata(
        const ShaderCodeModuleDefinition &definition,
        ShaderCodeModuleMetadataValidationDiagnostic &out_diagnostic) noexcept
    {
        out_diagnostic = {};
        out_diagnostic.module_name = definition.name ? definition.name : "";

        if (!IsValidShaderCodeModuleDefinition(definition))
            return SetMetadataFailure(
                out_diagnostic,
                ShaderCodeModuleMetadataValidationError::InvalidDefinition,
                definition.name);

        if (!HasValidArray(
                definition.semantic_requirements,
                definition.semantic_requirement_count)
         || !HasValidArray(
                definition.semantic_provides,
                definition.semantic_provide_count)
         || !HasValidArray(definition.dependencies, definition.dependency_count)
         || !HasValidArray(
                definition.module_conflict_names,
                definition.module_conflict_count)
         || !HasValidArray(
                definition.ssbo_requirements,
                definition.ssbo_requirement_count)
         || !HasValidArray(
                definition.texture_layer_requirements,
                definition.texture_layer_requirement_count))
        {
            return SetMetadataFailure(
                out_diagnostic,
                ShaderCodeModuleMetadataValidationError::InvalidArray,
                definition.name);
        }

        for (uint32 i = 0;
             i < definition.semantic_requirement_count;
             ++i)
        {
            const ShaderCodeModuleSemanticRequirement &requirement =
                definition.semantic_requirements[i];
            if (!IsValidRequirement(requirement))
                return SetMetadataFailure(
                    out_diagnostic,
                    ShaderCodeModuleMetadataValidationError::InvalidRequirement,
                    definition.name,
                    i,
                    nullptr,
                    requirement.semantic);

            for (uint32 j = 0; j < i; ++j)
            {
                const ShaderCodeModuleSemanticRequirement &other =
                    definition.semantic_requirements[j];
                if (requirement.source == other.source
                 && requirement.semantic == other.semantic)
                {
                    return SetMetadataFailure(
                        out_diagnostic,
                        ShaderCodeModuleMetadataValidationError::
                            DuplicateRequirement,
                        definition.name,
                        i,
                        nullptr,
                        requirement.semantic);
                }
            }
        }

        for (uint32 i = 0; i < definition.semantic_provide_count; ++i)
        {
            const ShaderCodeModuleSemantic semantic =
                definition.semantic_provides[i];
            if (semantic == ShaderCodeModuleSemantic::Unknown)
                return SetMetadataFailure(
                    out_diagnostic,
                    ShaderCodeModuleMetadataValidationError::InvalidRequirement,
                    definition.name,
                    i);

            for (uint32 j = 0; j < i; ++j)
            {
                if (semantic == definition.semantic_provides[j])
                    return SetMetadataFailure(
                        out_diagnostic,
                        ShaderCodeModuleMetadataValidationError::DuplicateProvide,
                        definition.name,
                        i,
                        nullptr,
                        semantic);
            }
        }

        const uint32 dependency_count =
            definition.dependency_count;
        for (uint32 i = 0; i < dependency_count; ++i)
        {
            // 内联访问（原 GetNormalizedShaderCodeModuleDependency 的边界防御
            // 由循环条件保证——i 恒 < dependency_count）
            const ShaderCodeModuleDependency &dependency =
                definition.dependencies[i];

            if (SameName(dependency.module_name, definition.name))
                return SetMetadataFailure(
                    out_diagnostic,
                    ShaderCodeModuleMetadataValidationError::SelfDependency,
                    definition.name,
                    i,
                    dependency.module_name);

            for (uint32 j = 0; j < i; ++j)
            {
                ShaderCodeModuleDependency other{};
                if (j < definition.dependency_count
                 && SameName(dependency.module_name,
                             definition.dependencies[j].module_name))
                {
                    return SetMetadataFailure(
                        out_diagnostic,
                        ShaderCodeModuleMetadataValidationError::
                            DuplicateDependency,
                        definition.name,
                        i,
                        dependency.module_name);
                }
            }
        }

        for (uint32 i = 0; i < definition.module_conflict_count; ++i)
        {
            const char *const conflict = definition.module_conflict_names[i];
            if (SameName(conflict, definition.name))
                return SetMetadataFailure(
                    out_diagnostic,
                    ShaderCodeModuleMetadataValidationError::SelfConflict,
                    definition.name,
                    i,
                    conflict);

            for (uint32 j = 0; j < i; ++j)
            {
                if (SameName(conflict, definition.module_conflict_names[j]))
                    return SetMetadataFailure(
                        out_diagnostic,
                        ShaderCodeModuleMetadataValidationError::
                            DuplicateConflict,
                        definition.name,
                        i,
                        conflict);
            }
        }

        return true;
    }

    bool ValidateShaderCodeModuleRegistryMetadata(
        const ShaderCodeModuleRegistry &registry,
        ShaderCodeModuleMetadataValidationDiagnostic &out_diagnostic) noexcept
    {
        out_diagnostic = {};

        for (int index = 0; index < registry.GetCount(); ++index)
        {
            const ShaderCodeModuleDefinition *definition =
                registry.GetModuleByIndex(index);
            if (!definition
             || !ValidateShaderCodeModuleMetadata(
                    *definition, out_diagnostic))
                return false;

            const uint32 dependency_count =
                definition->dependency_count;
            for (uint32 i = 0; i < dependency_count; ++i)
            {
                ShaderCodeModuleDependency dependency =
                    definition->dependencies[i];

                const ShaderCodeModuleDefinition *target =
                    registry.FindByName(dependency.module_name);
                if (!target)
                    return SetMetadataFailure(
                        out_diagnostic,
                        ShaderCodeModuleMetadataValidationError::MissingDependency,
                        definition->name,
                        i,
                        dependency.module_name);

            }

            for (uint32 i = 0; i < definition->module_conflict_count; ++i)
            {
                if (!registry.FindByName(definition->module_conflict_names[i]))
                    return SetMetadataFailure(
                        out_diagnostic,
                        ShaderCodeModuleMetadataValidationError::
                            MissingConflictTarget,
                        definition->name,
                        i,
                        definition->module_conflict_names[i]);
            }
        }

        ValueArray<uint8> visit_state;
        visit_state.Resize(registry.GetCount());
        for (int index = 0; index < visit_state.GetCount(); ++index)
            visit_state[index] = 0;

        for (int index = 0; index < registry.GetCount(); ++index)
        {
            if (visit_state[index] == 0
             && !ValidateDependencyVisit(
                    registry, index, visit_state, out_diagnostic))
                return false;
        }

        for (int left_index = 0;
             left_index < registry.GetCount();
             ++left_index)
        {
            const ShaderCodeModuleDefinition *left =
                registry.GetModuleByIndex(left_index);
            if (!left || !IsProviderCandidate(*left))
                continue;

            for (int right_index = left_index + 1;
                 right_index < registry.GetCount();
                 ++right_index)
            {
                const ShaderCodeModuleDefinition *right =
                    registry.GetModuleByIndex(right_index);
                if (!right
                 || !IsProviderCandidate(*right)
                 || left->priority != right->priority)
                    continue;

                for (uint32 left_provide = 0;
                     left_provide < left->semantic_provide_count;
                     ++left_provide)
                {
                    for (uint32 right_provide = 0;
                         right_provide < right->semantic_provide_count;
                         ++right_provide)
                    {
                        if (left->semantic_provides[left_provide]
                            == right->semantic_provides[right_provide])
                        {
                            return SetMetadataFailure(
                                out_diagnostic,
                                ShaderCodeModuleMetadataValidationError::
                                    AmbiguousProviderPriority,
                                left->name,
                                left_provide,
                                right->name,
                                left->semantic_provides[left_provide]);
                        }
                    }
                }
            }
        }

        return true;
    }
}
